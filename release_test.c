// Tests release.inc (the Windows updater's version, JSON, checksum and address
// rules) on the Mac:  cc -O2 -o build/release_test release_test.c && build/release_test [release.json]
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "release.inc"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void sha(const char *s, const char *want) {
    char hex[65]; relSha256((const unsigned char *)s, strlen(s), hex);
    if (strcmp(hex, want)) { printf("FAIL sha256(\"%.20s\"): %s\n", s, hex); fails++; }
}

int main(int argc, char **argv) {
    // versions
    CHECK(relNewer("1.4.0", "1.3.1")); CHECK(relNewer("v1.10.0", "1.9.2")); CHECK(!relNewer("1.3.1", "1.3.1"));
    CHECK(!relNewer("1.3.0", "1.3.1")); CHECK(relNewer("1.3.1.1", "1.3.1")); CHECK(!relNewer("1.3", "1.3.0"));
    CHECK(relNewer("2", "1.9.9")); CHECK(!relNewer("9.9.9", "dev")); CHECK(relIsVersion("1.4.0")); CHECK(!relIsVersion("1.4.0-beta"));

    // SHA-256 (FIPS 180-2 examples, and the 55/56/64-byte padding edges)
    sha("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    sha("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    sha("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    char big[1000001]; memset(big, 'a', 1000000); big[1000000] = 0;
    sha(big, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

    // SHA256SUMS
    const char *sums = "495d79cbfebd34fd40174cce86a155e5dc51f4a4898217f7fc131c7271f4e1dc  wasdmod-Windows.exe\n"
                       "16C051D357C354E26764AE78DBC0AAD1D1AC83D95A254935A3EF9D16489A67DC *wasdmod-Mac.dmg\r\n"
                       "e5bdfec74820f5bae87689faf20ba241a3c7448cb96521c8d0538de8eaa4b02f  wasdmod.zip\n";
    char hex[65];
    CHECK(relSumFor(sums, strlen(sums), "wasdmod-Windows.exe", hex) && !strcmp(hex, "495d79cbfebd34fd40174cce86a155e5dc51f4a4898217f7fc131c7271f4e1dc"));
    CHECK(relSumFor(sums, strlen(sums), "wasdmod-Mac.dmg", hex) && !strcmp(hex, "16c051d357c354e26764ae78dbc0aad1d1ac83d95a254935a3ef9d16489a67dc"));
    CHECK(!relSumFor(sums, strlen(sums), "wasdmod-Windows.ex", hex));
    CHECK(!relSumFor(sums, strlen(sums), "Windows.exe", hex));
    CHECK(!relSumFor("zz  wasdmod.zip\n", 16, "wasdmod.zip", hex));

    // addresses
    CHECK(relAllowedStart("https://github.com/Wanzho/mcd2-wasd/releases/download/v1.4.0/wasdmod-Windows.exe"));
    CHECK(!relAllowedStart("https://github.com/Wanzho/mcd2-wasd/releases/download/../../evil/x.exe"));
    CHECK(!relAllowedStart("http://github.com/Wanzho/mcd2-wasd/releases/download/v1.4.0/wasdmod-Windows.exe"));
    CHECK(!relAllowedStart("https://github.com/Wanzho/other/releases/download/v1.4.0/wasdmod-Windows.exe"));
    CHECK(!relAllowedStart("https://github.com.evil.example/Wanzho/mcd2-wasd/releases/download/v1/x"));
    CHECK(relAllowedHop("https://release-assets.githubusercontent.com/github-production-release-asset/1/2?sp=r&sig=x"));
    CHECK(relAllowedHop("https://objects.githubusercontent.com/x"));
    CHECK(relAllowedHop("https://GitHub.com/Wanzho/mcd2-wasd/releases/download/v1/x"));
    CHECK(!relAllowedHop("https://evil.example/x"));
    CHECK(!relAllowedHop("https://githubusercontent.com.evil.example/x"));
    CHECK(!relAllowedHop("https://github.com@evil.example/x"));
    CHECK(!relAllowedHop("http://objects.githubusercontent.com/x"));
    CHECK(!relAllowedHop("ftp://github.com/x"));
    relTestHost = "127.0.0.1:8123"; // a test build's local server
    CHECK(relAllowedHop("http://127.0.0.1:8123/x")); CHECK(!relAllowedHop("http://localhost:8123/x")); CHECK(!relAllowedHop("https://github.com/x"));
    relTestHost = 0;

    // the notes' summary
    char sum[480];
    relSummary("## What's new\r\n\r\n- **Drag keys around:** on the map, drag a key.\r\n- **No more jumping to the top:** in the apps.\r\n"
               "- **Typing mode and the cursor key** only take keyboard keys.\r\n\r\n## Downloads\r\n\r\n- **Windows:** `wasdmod-Windows.exe`\r\n", sum, sizeof(sum));
    CHECK(!strcmp(sum, "Drag keys around \xC2\xB7 No more jumping to the top \xC2\xB7 Typing mode and the cursor key"));
    relSummary("Fixes the [bow](https://x.y/z) aiming. And more.\n", sum, sizeof(sum));
    CHECK(!strcmp(sum, "Fixes the bow aiming"));
    relSummary("- one\n- two\n- three\n- four\n- five\n", sum, sizeof(sum));
    CHECK(!strcmp(sum, "one \xC2\xB7 two \xC2\xB7 three \xC2\xB7 four"));
    relSummary("", sum, sizeof(sum)); CHECK(!sum[0]);

    // a release
    const char *json = "{\"url\":\"x\",\"tag_name\":\"v1.4.0\",\"html_url\":\"https://github.com/Wanzho/mcd2-wasd/releases/tag/v1.4.0\","
        "\"author\":{\"login\":\"Wanzho\",\"a\":[1,2,{\"b\":null}]},\"draft\":false,\"body\":\"- **Caf\\u00e9 \\ud83c\\udfae** \\\"quoted\\\"\\n\","
        "\"assets\":[{\"name\":\"wasdmod-Windows.exe\",\"size\":1,\"browser_download_url\":\"https://github.com/Wanzho/mcd2-wasd/releases/download/v1.4.0/wasdmod-Windows.exe\"},"
        "{\"browser_download_url\":\"https://github.com/Wanzho/mcd2-wasd/releases/download/v1.4.0/SHA256SUMS\",\"name\":\"SHA256SUMS\"}],\"x\":-1.5e3}";
    RELINFO r;
    CHECK(relParse(json, strlen(json), "wasdmod-", ".exe", &r));
    CHECK(!strcmp(r.version, "1.4.0")); CHECK(!strcmp(r.page, "https://github.com/Wanzho/mcd2-wasd/releases/tag/v1.4.0"));
    CHECK(!strcmp(r.asset, "https://github.com/Wanzho/mcd2-wasd/releases/download/v1.4.0/wasdmod-Windows.exe"));
    CHECK(!strcmp(r.sums, "https://github.com/Wanzho/mcd2-wasd/releases/download/v1.4.0/SHA256SUMS"));
    CHECK(!strcmp(r.summary, "Caf\xC3\xA9 \xF0\x9F\x8E\xAE"));
    CHECK(!strcmp(r.assetName, "wasdmod-Windows.exe")); // an older release's name still works
    // versioned names: the one with the release's version wins, whatever the order; zips and odd names don't count
    const char *versioned = "{\"assets\":[{\"name\":\"wasdmod-1.3.9.exe\",\"browser_download_url\":\"https://github.com/Wanzho/mcd2-wasd/releases/download/v1.4.0/wasdmod-1.3.9.exe\"},"
        "{\"name\":\"wasdmod-1.4.0.zip\",\"browser_download_url\":\"https://github.com/Wanzho/mcd2-wasd/releases/download/v1.4.0/wasdmod-1.4.0.zip\"},"
        "{\"name\":\"wasdmod-x/../1.exe\",\"browser_download_url\":\"https://github.com/Wanzho/mcd2-wasd/releases/download/v1.4.0/x.exe\"},"
        "{\"name\":\"wasdmod-1.4.0.exe\",\"browser_download_url\":\"https://github.com/Wanzho/mcd2-wasd/releases/download/v1.4.0/wasdmod-1.4.0.exe\"},"
        "{\"name\":\"wasdmod-1.4.0.dmg\",\"browser_download_url\":\"https://github.com/Wanzho/mcd2-wasd/releases/download/v1.4.0/wasdmod-1.4.0.dmg\"}],\"tag_name\":\"v1.4.0\"}";
    CHECK(relParse(versioned, strlen(versioned), "wasdmod-", ".exe", &r) && !strcmp(r.assetName, "wasdmod-1.4.0.exe")
          && !strcmp(r.asset, "https://github.com/Wanzho/mcd2-wasd/releases/download/v1.4.0/wasdmod-1.4.0.exe"));
    CHECK(relParse(versioned, strlen(versioned), "wasdmod-", ".dmg", &r) && !strcmp(r.assetName, "wasdmod-1.4.0.dmg"));
    CHECK(relAssetLike("wasdmod-1.4.0.exe", "wasdmod-", ".exe") && !relAssetLike("wasdmod-.exe", "wasdmod-", ".exe")
          && !relAssetLike("wasdmod-1.4.0.exe.zip", "wasdmod-", ".exe") && !relAssetLike("evil-1.4.0.exe", "wasdmod-", ".exe"));
    const char *evil = "{\"tag_name\":\"v2.0.0\",\"html_url\":\"https://evil.example/\",\"assets\":[]}";
    CHECK(relParse(evil, strlen(evil), "wasdmod-", ".exe", &r) && !strcmp(r.page, REL_PAGES) && !r.asset[0] && !r.sums[0]);
    CHECK(!relParse("{\"tag_name\":\"latest\"}", 21, "wasdmod-", ".exe", &r));
    CHECK(!relParse("{\"message\":\"API rate limit exceeded\"}", 38, "wasdmod-", ".exe", &r));
    CHECK(!relParse("{\"tag_name\":\"v1.4.0\",", 21, "wasdmod-", ".exe", &r));
    CHECK(!relParse("<html>", 6, "wasdmod-", ".exe", &r));

    // GitHub's real answer, if given (curl .../releases/latest > file)
    if (argc > 1) {
        FILE *f = fopen(argv[1], "rb"); static char buf[1 << 20]; size_t n = f ? fread(buf, 1, sizeof(buf), f) : 0; if (f) fclose(f);
        CHECK(relParse(buf, (unsigned)n, "wasdmod-", ".exe", &r));
        printf("real release: %s, page %s\n  asset %s\n  sums %s\n  summary: %s\n", r.version, r.page, r.asset[0] ? r.asset : "(none)", r.sums[0] ? r.sums : "(none)", r.summary);
    }
    printf(fails ? "%d FAILED\n" : "release.inc: all tests passed\n", fails);
    return fails != 0;
}
