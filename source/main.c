// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include the main libnx system header, for Switch development
#include <switch.h>

void showError(char* errorText1, char* errorText2, Result outrc)
{
    AppletHolder err;
    AppletStorage errStor;
    LibAppletArgs errArgs;

    appletCreateLibraryApplet(&err, AppletId_LibraryAppletError, LibAppletMode_AllForeground);
    libappletArgsCreate(&errArgs, 1);
    libappletArgsPush(&errArgs, &err);
    appletCreateStorage(&errStor, 4120);
    u8 args[4120] = {0};
    args[0] = 1;

    u64 e = (((outrc & 0x1ffu) + 2000) | (((outrc >> 9) & 0x1fff & 0x1fffll) << 32));
    *(u64*)&args[8] = e;
    strcpy((char*) &args[24], errorText1);
    strcpy((char*) &args[2072], errorText2);
    appletStorageWrite(&errStor, 0, args, 4120);
    appletHolderPushInData(&err, &errStor);

    appletHolderStart(&err);
    appletHolderJoin(&err);
    exit(1);
}

SwkbdTextCheckResult validate_text(char* tmp_string, size_t tmp_string_size) {
    if (strncmp(tmp_string, "https://", 8) !=0 && strncmp(tmp_string, "http://", 7) !=0) {
        strncpy(tmp_string, "请在网址前添加 'https://' 或 'http://'", tmp_string_size);
        return SwkbdTextCheckResult_Bad;
    }
    if(strlen(tmp_string) < 12) {
        strncpy(tmp_string, "网址无效", tmp_string_size);
        return SwkbdTextCheckResult_Bad;
    }
    static const char *allowed=":/.%0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char *reset = tmp_string;
    while(*tmp_string)
    {
        if(strchr(allowed, *tmp_string) == NULL){
            strncpy(reset, "请从网址中删除无效的字符", tmp_string_size);
            return SwkbdTextCheckResult_Bad;
        }
        tmp_string++;
    }

    return SwkbdTextCheckResult_OK;
}

int showKeyboard(char out[0xc00], char* title, char* placehold, char* oktxt, char* initial)
{
    int rc;
    SwkbdConfig kbd;
    char tmpoutstr[0xc00] = {0};
    rc = swkbdCreate(&kbd, 0);
    if(R_SUCCEEDED(rc)) {
        swkbdConfigMakePresetDefault(&kbd);
        swkbdConfigSetHeaderText(&kbd, title);
        swkbdConfigSetOkButtonText(&kbd, oktxt);
        swkbdConfigSetGuideText(&kbd, placehold);
        swkbdConfigSetInitialText(&kbd, initial);
        swkbdConfigSetTextCheckCallback(&kbd, validate_text);
        rc = swkbdShow(&kbd, tmpoutstr, sizeof(tmpoutstr));
        if(R_SUCCEEDED(rc)) {
            if(tmpoutstr[0] != '\0') {
                strcpy(out, tmpoutstr);
                swkbdClose(&kbd);
                return 0;
            }
        }else if(rc != (Result) 0x5d59) {
            showError("软件键盘错误\n查看错误代码以获取更多信息", "", rc);
        }
    }else{
        showError("软件键盘错误\n查看错误代码以获取更多信息", "", rc);
    }
    swkbdClose(&kbd);
    return -1;
}
void startAuthApplet(char* url) {
    Result rc=0;
    // TODO: Move to libnx impl once its in a stable release
    WebWifiConfig config;
    webWifiCreate(&config, NULL, url, (Uuid) {0} , 0);
    WebWifiReturnValue out;
    rc = webWifiShow(&config, &out);
    if(R_FAILED(rc)) {
        showError("浏览器错误\n查看错误代码以获取更多信息", "", rc);
    }
}

int main(int argc, char* argv[])
{
    Result rc=0;
    int i = 0;
    char url[0xc00] = {0};
    nsvmInitialize();
    pminfoInitialize();
    consoleInit(NULL);

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    strcpy(url, "https://cuojue.org");
    printf("按 [L] 选择网址\n");
    printf("按 [R] 设置默认网址\n");
    printf("按 [X] 重置默认网址\n");
    printf("按 [-] 以认证程序方式启动 " CONSOLE_RED "(功能有限)");
    bool nagOn;
    nsvmNeedsUpdateVulnerability(&nagOn);
    if(nagOn) {
        s32 pCount;
        u64 pids[100];
        u64 cId;
        u32 i = 0;
        bool isPatched = false;
        svcGetProcessList(&pCount, pids, 100);
        while (i <= pCount-1) {
            pminfoGetProgramId(&cId, pids[i]);
            if(cId == 0x00FF747765616BFF || cId == 0x01FF415446660000) {
                printf(CONSOLE_GREEN "Supernag 已启用，但已通过 switch-sys-tweak 修补！\n");
                isPatched = true;
                break;
            }
            i++;
        }
        if(!isPatched){
            showError("错误：Nag 已激活，请查看更多详情", "浏览器在 Nag 激活时无法启动\n\n请使用 gagorder 或 switch-sys-tweak（后者随 BrowseNX 一起捆绑）来禁用 Nag。", 0);
        }
    }
    bool forceAuth = false;
    while(appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if(kDown & HidNpadButton_L) {
            showKeyboard(url, "选择网址", "输入网址", "前往", "https://");
            break;
        } else if(kDown & HidNpadButton_R) {
            char defUrl[0xc00] = {0};
            char fURL[0xc00] = {0};
            FILE* dUCheck = fopen("sdmc:/config/BrowseNX/defUrl.txt", "r");
            if (dUCheck != NULL) {
                fgets(fURL, 0xc00, dUCheck);
            }else {
                strcpy(fURL, url);
            }
            fclose(dUCheck);
            if(showKeyboard(defUrl, "设置默认网址", "输入网址", "设置", fURL) == 0) {
                FILE* dUFile = fopen("sdmc:/config/BrowseNX/defUrl.txt", "w+");
                fprintf(dUFile, "%s", defUrl);
                fclose(dUFile);
            }
        } else if(kDown & HidNpadButton_X) {
            remove("sdmc:/config/BrowseNX/defUrl.txt");
        } else if(kDown & HidNpadButton_Plus) {
            forceAuth = true;
        }
        i++;
        if(i >= 60*2){
            break;
        }
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    pminfoExit();
    nsvmExit();

    FILE* dUFile = fopen("sdmc:/config/BrowseNX/defUrl.txt", "r");
    char fURL[0xc00] = {0};
    if(dUFile != NULL) {
        fgets(fURL, 0xc00, dUFile);
        fclose(dUFile);
        if(validate_text(fURL, 0xc00) == SwkbdTextCheckResult_OK) {
            strcpy(url, fURL);
        }else{
            showError("默认网址文件错误，请查看更多详情", fURL, 0);
        }
    }
    if(appletGetAppletType() == AppletType_Application && !forceAuth) { // Running as a title
        WebCommonConfig conf;
        WebCommonReply out;
        rc = webPageCreate(&conf, url);
        if (R_FAILED(rc))
            showError("浏览器启动错误\n查看错误代码以获取更多信息", "", rc);
        webConfigSetJsExtension(&conf, true);
        webConfigSetPageCache(&conf, true);
        webConfigSetBootLoadingIcon(&conf, true);
        webConfigSetWhitelist(&conf, ".*");
        rc = webConfigShow(&conf, &out);
        if (R_FAILED(rc))
            showError("浏览器启动错误\n查看错误代码以获取更多信息", "", rc);
    } else { // Running under applet
        showError("以程序模式运行\n请通过在应用程序（例如游戏）上按住 R 键启动 hbmenu，而不是在小程序（例如图库）上启动", "", 0);
    }
    return 0;
}
