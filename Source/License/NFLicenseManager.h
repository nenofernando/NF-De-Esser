#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <memory>
#include <thread>
#include "NFLicenseVerify.h"
#include "MachineFingerprint.h"

/** Gerencia o estado de licença de UM produto NF (identificado por
    productCode, ex: "NF_VOCAL_COMPRESSOR") - ativação online (só na hora
    de ativar, precisa de internet) e verificação offline depois disso
    (todo carregamento do plugin só relê e reverifica o certificado local,
    sem chamar o servidor).

    Pensado pra ser reutilizável entre os plugins da linha NF - só muda o
    productCode que cada plugin passa no construtor.
*/
class NFLicenseManager
{
public:
    static constexpr const char* apiBaseUrl = "https://xgbqtmspfwrcpxoyvxvl.supabase.co/functions/v1";

    explicit NFLicenseManager (juce::String productCodeIn) : productCode (std::move (productCodeIn))
    {
        loadAndVerifyLocalCertificate();
    }

    // activateAsync() hands the detached worker thread a copy of aliveFlag;
    // this destructor flips it so the thread's message-thread callback can
    // tell `this` no longer exists (e.g. plugin closed mid-activation) and
    // skip touching it, instead of dereferencing a dangling pointer.
    ~NFLicenseManager() { *aliveFlag = false; }

    /** true só se existe um certificado local, a assinatura bate, o
        produto bate, e a máquina bate com o fingerprint atual. */
    bool isActivated() const { return activated.load (std::memory_order_acquire); }

    /** Relê e reverifica o certificado local (message thread). O resultado vira visível pra thread de áudio numa única
        escrita atômica no fim - nunca passa por um "desativado" intermediário. */
    void reloadLocalCertificate() { loadAndVerifyLocalCertificate(); }

    /** true se o certificado (JSON) é do produto e da máquina esperados. Separado da assinatura (que é verificada à parte)
        pra poder ser testado sozinho. */
    static bool validateCertificateFields (const juce::String& certificate, const juce::String& product, const juce::String& machineId)
    {
        auto certVar = juce::JSON::parse (certificate);
        return certVar.getProperty ("product_code", "").toString() == product
            && certVar.getProperty ("machine_id", "").toString() == machineId;
    }

    /** Grava o certificado em `file`. Devolve false se não deu pra criar a pasta, escrever ou reler o arquivo - quem chama
        NÃO pode dizer "ativado" nesse caso. */
    static bool saveCertificateTo (const juce::File& file, const juce::String& certificate, const juce::String& signature)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("certificate", certificate);
        obj->setProperty ("signature", signature);

        auto dir = file.getParentDirectory();
        if (! dir.exists() && ! dir.createDirectory().wasOk())
            return false;
        if (! file.replaceWithText (juce::JSON::toString (juce::var (obj), true)))
            return false;
        return file.existsAsFile();
    }

    /** Dispara a ativação em background (não trava GUI/audio thread).
        `onResult` é chamado na message thread quando terminar. */
    void activateAsync (const juce::String& licenseKey,
                         std::function<void (bool success, juce::String errorMessage)> onResult)
    {
        auto fingerprint = MachineFingerprint::get();
        auto product = productCode;
        auto base = juce::String (apiBaseUrl);
        auto alive = aliveFlag;

        std::thread worker ([fingerprint, product, licenseKey, base, onResult, alive, this]
        {
            // Chamada estatica: nao toca em nenhum estado da instancia, entao
            // e segura mesmo que `this` seja destruido antes de terminar.
            auto result = performActivationRequest (base, licenseKey, product, fingerprint);

            juce::MessageManager::callAsync ([alive, this, result, onResult]
            {
                bool ok = result.success;
                juce::String error = result.errorMessage;

                if (*alive && ok)
                {
                    loadAndVerifyLocalCertificate();
                    // Só diz "ativado" se a licença realmente ficou valendo neste computador.
                    if (! isActivated())
                    {
                        ok = false;
                        error = "A licenca foi recebida, mas nao pode ser confirmada neste computador.";
                    }
                }

                if (onResult)
                    onResult (ok, error);
            });
        });
        worker.detach();
    }

private:
    struct ActivationResult
    {
        bool success = false;
        juce::String errorMessage;
    };

    static ActivationResult performActivationRequest (const juce::String& base, const juce::String& licenseKey,
                                                        const juce::String& product, MachineFingerprint::Result fingerprint)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("license_key", licenseKey.trim());
        obj->setProperty ("product_code", product);
        obj->setProperty ("machine_id", fingerprint.machineId);
        obj->setProperty ("machine_id_fallback", fingerprint.isFallback);
        obj->setProperty ("machine_label", juce::SystemStats::getComputerName());
       #if JUCE_WINDOWS
        obj->setProperty ("os", "windows");
       #elif JUCE_MAC
        obj->setProperty ("os", "macos");
       #endif

        juce::var bodyVar (obj);
        auto bodyJson = juce::JSON::toString (bodyVar, true);

        juce::URL url (base + "/license-activate");
        url = url.withPOSTData (bodyJson);

        int statusCode = 0;
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                           .withExtraHeaders ("Content-Type: application/json")
                           .withConnectionTimeoutMs (10000)
                           .withStatusCode (&statusCode);

        auto stream = url.createInputStream (options);
        if (stream == nullptr)
            return { false, "Nao foi possivel conectar ao servidor de licenca." };

        auto response = stream->readEntireStreamAsString();
        auto responseVar = juce::JSON::parse (response);

        if (statusCode != 200)
        {
            auto errorField = responseVar.getProperty ("error", "erro_desconhecido").toString();
            return { false, errorField };
        }

        auto certificate = responseVar.getProperty ("certificate", "").toString();
        auto signature = responseVar.getProperty ("signature", "").toString();

        if (certificate.isEmpty() || signature.isEmpty())
            return { false, "Resposta invalida do servidor." };

        if (! NFLicenseVerify::verify (certificate, signature))
            return { false, "Certificado recebido nao pode ser verificado." };

        if (! validateCertificateFields (certificate, product, fingerprint.machineId))
            return { false, "O certificado recebido nao corresponde a este produto ou a este computador." };

        if (! saveCertificateTo (getCertificateFile (product), certificate, signature))
            return { false, "Nao foi possivel gravar a licenca neste computador." };

        return { true, {} };
    }

    void loadAndVerifyLocalCertificate()
    {
        // Tudo é calculado numa variável local e publicado de uma vez: a thread de áudio nunca vê um "desativado" no meio.
        activated.store (checkLocalCertificate (productCode), std::memory_order_release);
    }

    static bool checkLocalCertificate (const juce::String& product)
    {
        auto file = getCertificateFile (product);
        if (! file.existsAsFile())
            return false;

        auto stored = juce::JSON::parse (file);
        auto certificate = stored.getProperty ("certificate", "").toString();
        auto signature = stored.getProperty ("signature", "").toString();

        if (certificate.isEmpty() || signature.isEmpty())
            return false;

        if (! NFLicenseVerify::verify (certificate, signature))
            return false;

        return validateCertificateFields (certificate, product, MachineFingerprint::get().machineId);
    }

    static juce::File getCertificateFile (const juce::String& productCode)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("NF Plugins")
                       .getChildFile ("licenses");
        return dir.getChildFile (productCode + ".lic");
    }

    std::shared_ptr<std::atomic<bool>> aliveFlag = std::make_shared<std::atomic<bool>> (true);
    juce::String productCode;
    std::atomic<bool> activated { false };   // escrito pela thread da interface, lido pela thread de áudio
};
