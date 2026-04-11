#include <3ds.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Old 3DS-friendly bootstrap: simple console UI, light rendering, minimal state.
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    printf("Hermes Agent 3DS\n");
    printf("\n");
    printf("Bootstrap build is alive.\n");
    printf("\n");
    printf("This first app scaffold keeps things simple for Old 3DS hardware.\n");
    printf("\n");
    printf("Next up:\n");
    printf("- settings\n");
    printf("- bridge health check\n");
    printf("- chat flow\n");
    printf("\n");
    printf("Press START to exit.\n");

    while (aptMainLoop())
    {
        gspWaitForVBlank();
        hidScanInput();

        u32 kDown = hidKeysDown();
        if (kDown & KEY_START)
            break;

        gfxFlushBuffers();
        gfxSwapBuffers();
    }

    gfxExit();
    return 0;
}
