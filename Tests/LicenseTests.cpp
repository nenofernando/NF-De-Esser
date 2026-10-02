#undef NDEBUG   // the checks must run in Release builds too
// Regression tests for the licence code (audit of 2026-10-02): the activated flag is shared between the UI thread and the audio
// thread, and an activation may only be reported as a success when the licence really ended up valid on this computer.
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "../Source/License/NFLicenseManager.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

int main()
{
    // 1) The certificate must be for THIS product and THIS machine.
    {
        const juce::String cert = R"({"product_code":"NF_DE_ESSER","machine_id":"abc123"})";
        assert(NFLicenseManager::validateCertificateFields(cert, "NF_DE_ESSER", "abc123"));
        assert(! NFLicenseManager::validateCertificateFields(cert, "NF_GLUE", "abc123"));        // another product
        assert(! NFLicenseManager::validateCertificateFields(cert, "NF_DE_ESSER", "other-pc"));  // another computer
        assert(! NFLicenseManager::validateCertificateFields("not json", "NF_DE_ESSER", "abc123"));
        assert(! NFLicenseManager::validateCertificateFields("{}", "NF_DE_ESSER", "abc123"));
    }

    // 2) Saving must report failure honestly (it used to be ignored, and the activation still said "success").
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("nf-license-test-" + juce::String(juce::Random::getSystemRandom().nextInt()));
        auto file = dir.getChildFile("a").getChildFile("b").getChildFile("TEST.lic");
        assert(NFLicenseManager::saveCertificateTo(file, "{\"x\":1}", "c2lnbmF0dXJl"));          // creates the folders, writes, re-reads
        auto stored = juce::JSON::parse(file);
        assert(stored.getProperty("certificate", "").toString() == "{\"x\":1}");
        // the parent "folder" is a regular file: the folder cannot be created, so saving has to fail
        auto blocker = dir.getChildFile("blocker");
        assert(blocker.replaceWithText("x"));
        assert(! NFLicenseManager::saveCertificateTo(blocker.getChildFile("TEST.lic"), "{}", "sig"));
        dir.deleteRecursively();
    }

    // 3) A forged or empty signature is refused.
    assert(! NFLicenseVerify::verify("{}", "AAAA"));
    assert(! NFLicenseVerify::verify("{\"product_code\":\"NF_DE_ESSER\"}", ""));

    // 4) No certificate on disk = not activated, and reloading never flips it to "activated" by itself.
    NFLicenseManager manager("NF_TEST_PRODUCT_WITHOUT_CERTIFICATE");
    assert(! manager.isActivated());

    // 5) The flag is read by the audio thread while the UI thread reloads it: must be race-free (run this under ThreadSanitizer).
    {
        std::atomic<bool> stop { false };
        std::atomic<long> reads { 0 }, wrongReads { 0 };
        std::thread audio([&] { while (! stop.load()) { if (manager.isActivated()) wrongReads.fetch_add(1); reads.fetch_add(1); } });
        for (int i = 0; i < 2000; ++i) manager.reloadLocalCertificate();
        stop = true; audio.join();
        assert(reads.load() > 0);
        assert(wrongReads.load() == 0);   // with no certificate it must read "not activated" every single time, never a flicker
    }

    std::cout << "NF De-Esser licence tests passed\n";
    return 0;
}
