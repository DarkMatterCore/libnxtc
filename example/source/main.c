#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <nxtc.h>

#define NS_APPLICATION_RECORD_BLOCK_SIZE    1024

static const char* g_langNames[SetLanguage_Total] = {
    [SetLanguage_JA]     = "Japanese",
    [SetLanguage_ENUS]   = "American English",
    [SetLanguage_FR]     = "French",
    [SetLanguage_DE]     = "German",
    [SetLanguage_IT]     = "Italian",
    [SetLanguage_ES]     = "Spanish",
    [SetLanguage_ZHCN]   = "Simplified Chinese",
    [SetLanguage_KO]     = "Korean",
    [SetLanguage_NL]     = "Dutch",
    [SetLanguage_PT]     = "Portuguese",
    [SetLanguage_RU]     = "Russian",
    [SetLanguage_ZHTW]   = "Traditional Chinese",
    [SetLanguage_ENGB]   = "British English",
    [SetLanguage_FRCA]   = "Canadian French",
    [SetLanguage_ES419]  = "Latin American Spanish",
    [SetLanguage_ZHHANS] = "Simplified Chinese",
    [SetLanguage_ZHHANT] = "Traditional Chinese",
    [SetLanguage_PTBR]   = "Brazilian Portuguese"
};

static bool utilsGetApplicationIds(u64 **out_application_ids, u32 *out_application_id_count)
{
    Result rc = 0;

    NsApplicationRecord *app_records = NULL, *tmp_app_records = NULL;
    u32 app_records_block_count = 0, app_records_count = 0;
    size_t app_records_size = 0, app_records_block_size = (NS_APPLICATION_RECORD_BLOCK_SIZE * sizeof(NsApplicationRecord));

    u64 *application_ids = NULL;

    bool success = false;

    /* Retrieve NS application records in a loop until we get them all. */
    do {
        /* Allocate memory for the NS application records. */
        tmp_app_records = realloc(app_records, app_records_size + app_records_block_size);
        if (!tmp_app_records) goto end;

        app_records = tmp_app_records;
        tmp_app_records = NULL;
        app_records_size += app_records_block_size;

        /* Clear newly allocated block. */
        NsApplicationRecord *app_records_block = &(app_records[app_records_count]);
        memset(app_records_block, 0, app_records_block_size);

        /* Retrieve NS application records. */
        rc = nsListApplicationRecord(app_records_block, NS_APPLICATION_RECORD_BLOCK_SIZE, (s32)app_records_count, (s32*)&app_records_block_count);
        if (R_FAILED(rc))
        {
            if (!app_records_count) goto end;
            break; /* Gotta work with what we have. */
        }

        app_records_count += app_records_block_count;
    } while(app_records_block_count >= NS_APPLICATION_RECORD_BLOCK_SIZE);

    /* Return right away if no records are available. */
    if (!app_records_count)
    {
        *out_application_ids = NULL;
        *out_application_id_count = 0;
        success = true;
        goto end;
    }

    /* Allocate memory for the output buffer. */
    application_ids = calloc(app_records_count, sizeof(u64));
    if (!application_ids) goto end;

    /* Populate output buffer. */
    for(u32 i = 0; i < app_records_count; i++) application_ids[i] = app_records[i].application_id;

    /* Update output. */
    *out_application_ids = application_ids;
    *out_application_id_count = app_records_count;

    /* Update flag. */
    success = true;

end:
    if (app_records) free(app_records);

    return success;
}

static bool utilsGetControlData(u64 application_id, NsApplicationControlData *out_ns_control_data, u64 *out_ns_control_data_size)
{
    Result rc = 0;
    u64 ns_control_data_size = 0;

    /* Retrieve application control data from ns. */
    /* Extremely slow under HOS 20.0.0+. */
    rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, application_id, out_ns_control_data, sizeof(NsApplicationControlData), &ns_control_data_size);
    if (R_FAILED(rc)) return false;

    /* Sanity check. */
    if (ns_control_data_size < sizeof(NacpStruct)) return false;

    /* Update output size. */
    *out_ns_control_data_size = ns_control_data_size;

    return true;
}

static NxTitleCacheApplicationMetadata *utilsGenerateApplicationMetadataEntryFromNsControlData(u64 application_id)
{
    NsApplicationControlData ns_control_data = {0};
    u64 ns_control_data_size = 0, icon_size = 0;

    NxTitleCacheApplicationMetadata *app_metadata = NULL;

    /* Retrieve application control data from ns. */
    if (!utilsGetControlData(application_id, &ns_control_data, &ns_control_data_size)) goto end;

    /* Calculate icon size. */
    icon_size = (ns_control_data_size - sizeof(NacpStruct));

    /* Update title cache using the control data we just retrieved. */
    if (!nxtcAddEntry(application_id, &(ns_control_data.nacp), icon_size, ns_control_data.icon, false)) goto end;

    /* Get application metadata from our cache. */
    app_metadata = nxtcGetApplicationMetadataEntryById(application_id);

end:
    return app_metadata;
}

NX_INLINE void utilsPrintApplicationMetadataInfo(NxTitleCacheApplicationMetadata *app_metadata, bool is_cache)
{
    printf("\t\t- %016lX: OK!\n" \
           "\t\t\t- Source: %s\n" \
           "\t\t\t- Language: %u (%s)\n" \
           "\t\t\t- Name: %s\n" \
           "\t\t\t- Publisher: %s\n" \
           "\t\t\t- Icon size: 0x%lX\n", \
           app_metadata->title_id, is_cache ? "cache" : "ns", app_metadata->language, g_langNames[app_metadata->language], \
           app_metadata->name, app_metadata->publisher, app_metadata->icon_size);
}

static bool utilsGetApplicationMetadata(u64 application_id)
{
    NxTitleCacheApplicationMetadata *app_metadata = NULL;
    bool success = false;

    /* Retrieve application metadata from our cache. */
    app_metadata = nxtcGetApplicationMetadataEntryById(application_id);
    if (app_metadata)
    {
        /* Print metadata. */
        utilsPrintApplicationMetadataInfo(app_metadata, true);
        success = true;
        goto end;
    }

    /* Generate application metadata using control data from ns. */
    app_metadata = utilsGenerateApplicationMetadataEntryFromNsControlData(application_id);
    if (!app_metadata)
    {
        printf("\t\t- %016lX: FAILED.\n", application_id);
        goto end;
    }

    /* Print metadata. */
    utilsPrintApplicationMetadataInfo(app_metadata, false);

    /* Update flag. */
    success = true;

end:
    consoleUpdate(NULL);

    if (app_metadata) nxtcFreeApplicationMetadata(&app_metadata);

    return success;
}

int main(int argc, char **argv)
{
    NX_IGNORE_ARG(argc);
    NX_IGNORE_ARG(argv);

    Result rc = 0;
    int ret = 0;
    PadState pad = {0};

    u64 *application_ids = NULL;
    u32 application_id_count = 0;

    /* Initialize console output. */
    consoleInit(NULL);

    /* Configure our supported input layout: a single player with full controller styles. */
    padConfigureInput(1, HidNpadStyleSet_NpadFullCtrl);

    /* Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller. */
    padInitializeDefault(&pad);

    printf(APP_TITLE ". Built on " BUILD_TIMESTAMP ".\nLibrary version: %u.%u.%u.\n\n", LIBNXTC_VERSION_MAJOR, LIBNXTC_VERSION_MINOR, LIBNXTC_VERSION_MICRO);
    consoleUpdate(NULL);

    /* Initialize ns services. */
    printf("\t- Initializing ns services: ");
    consoleUpdate(NULL);

    rc = nsInitialize();
    if (R_FAILED(rc))
    {
        printf("FAILED!\n");
        ret = -1;
        goto end;
    }

    printf("OK!\n");
    consoleUpdate(NULL);

    /* Initialize title cache interface. */
    printf("\t- Initializing title cache interface: ");
    consoleUpdate(NULL);

    if (!nxtcInitialize())
    {
        printf("FAILED!\n");
        ret = -2;
        goto end;
    }

    printf("OK!\n");
    consoleUpdate(NULL);

    /* Retrieve application IDs. */
    printf("\nPress A to retrieve application IDs.\n\n");
    consoleUpdate(NULL);

    while(appletMainLoop())
    {
        padUpdate(&pad);

        u64 keys_down = padGetButtonsDown(&pad);
        if (keys_down & HidNpadButton_A) break;

        svcSleepThread(10000000ULL);
    }

    /* Retrieve application IDs. */
    printf("\t- Retrieving application IDs: ");
    consoleUpdate(NULL);

    if (!utilsGetApplicationIds(&application_ids, &application_id_count))
    {
        printf("FAILED!\n");
        ret = -3;
        goto end;
    }

    printf("OK! Retrieved %u ID(s).\n", application_id_count);
    consoleUpdate(NULL);

    if (!application_id_count) goto btn_prompt;

    /* Retrieve application metadata. */
    printf("\t- Retrieving application metadata:\n\n");
    consoleUpdate(NULL);

    for(u32 i = 0; i < application_id_count; i++)
    {
        utilsGetApplicationMetadata(application_ids[i]);
        svcSleepThread(1000000ULL);
    }

    /* Flush title cache file. */
    printf("\t- Flushing title cache file... ");
    consoleUpdate(NULL);

    nxtcFlushCacheFile();

    printf("done.\n");
    consoleUpdate(NULL);

btn_prompt:
    printf("\nPress + to exit.\n\n");
    consoleUpdate(NULL);

    while(appletMainLoop())
    {
        padUpdate(&pad);

        u64 keys_down = padGetButtonsDown(&pad);
        if (keys_down & HidNpadButton_Plus) break;

        svcSleepThread(10000000ULL);
    }

    /* Deinitialize title cache interface. */
    nxtcExit();

end:
    /* Update console output. */
    consoleUpdate(NULL);

    /* Wait some time (3 seconds). */
    if (ret != 0) svcSleepThread(3000000000ULL);

    /* Close ns services. */
    nsExit();

    /* Deinitialize console output. */
    consoleExit(NULL);

    return ret;
}
