file(GLOB_RECURSE HELIX_SRCS 
    "src/audio/libhelix/src/libhelix-mp3/*.c" 
    "src/audio/libhelix/src/libhelix-aac/*.c"
    "src/audio/libhelix/src/utils/*.cpp"
    "src/audio/libhelix/src/utils/*.c"
)

file(GLOB_RECURSE LUA_SRCS
    "src/lua/liblua/*.c"
)

idf_component_register(
    SRCS 
        "src/cbdos_core.cpp"
        "src/ui/ThemeEngine.cpp"
        "src/ui/UIManager.cpp"
        "src/ui/WallpaperManager.cpp"
        "src/ui/themes/DefaultTheme.cpp"
        "src/ui/assets/default_wallpaper.c"
        "src/ui/components/HeaderBar.cpp"
        "src/ui/components/QuickSettingsPanel.cpp"
        "src/ui/views/DashboardView.cpp"
        "src/ui/views/SplashScreenView.cpp"
        "src/ui/views/ConfigView.cpp"
        "src/ui/views/WiFiConfigView.cpp"
        "src/ui/views/WallpaperConfigView.cpp"
        "src/ui/views/StorageConfigView.cpp"
        "src/ui/views/MusicPlayerView.cpp"
        "src/ui/views/RadioView.cpp"
        "src/ui/views/FlasherView.cpp"
        "src/ui/views/LuaRunnerView.cpp"
        "src/ui/modals/DiagnosticsModal.cpp"

        "src/ui/modals/C6FlasherModal.cpp"
        "src/audio/AudioPlayer.cpp"
        "src/audio/RadioManager.cpp"

        "src/network/ConfigManager.cpp"
        "src/network/TimeService.cpp"
        "src/lua/LuaEngine.cpp"
        "src/lua/LuaBridge.cpp"
        "src/lua/LuaRunner.cpp"
        ${HELIX_SRCS}
        ${LUA_SRCS}
    INCLUDE_DIRS 
        "include"
        "src/ui"
        "src/audio"
        "src/audio/libhelix/src"
        "src/audio/libhelix/src/utils"
        "src/audio/libhelix/src/libhelix-mp3"
        "src/audio/libhelix/src/libhelix-aac"
        "src/network"
        "src/lua"
        "src/lua/liblua"
    REQUIRES 
        "esp_timer"
        "freertos"
        "lvgl__lvgl"
        "nvs_flash"
        "json"
        "esp_http_client"
        "driver"
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC -DESP32 -DUSE_DEFAULT_STDLIB -DHELIX_LOGGING_ACTIVE=0)
target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-error -Wno-format -Wno-unused-variable)
