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
        "src/ui/views/ConfigView.cpp"
        "src/ui/views/WiFiConfigView.cpp"
        "src/ui/views/WallpaperConfigView.cpp"
        "src/ui/views/StorageConfigView.cpp"
        "src/ui/views/TimeConfigView.cpp"
        "src/ui/views/MusicPlayerView.cpp"
        "src/ui/views/GalleryListView.cpp"
        "src/ui/views/GalleryView.cpp"
        "src/ui/views/RadioView.cpp"
        "src/ui/views/FlasherView.cpp"
        "src/ui/views/LuaRunnerView.cpp"
        "src/ui/views/TextEditorView.cpp"
        "src/ui/views/FileManagerView.cpp"
        "src/ui/views/CartridgeView.cpp"
        "src/ui/views/SerialTerminalView.cpp"
        "src/ui/views/UtilitiesView.cpp"
        "src/ui/views/TlvBrowserView.cpp"
        "src/ui/views/MeshConfigView.cpp"
        "src/ui/views/RadioConfigView.cpp"
        "src/ui/views/utilities/CalculatorApp.cpp"
        "src/ui/views/utilities/StopwatchApp.cpp"
        "src/ui/views/utilities/PomodoroApp.cpp"
        "src/ui/views/utilities/TodoApp.cpp"
        "src/cartridge/CartridgeManager.cpp"
        "src/ui/modals/DiagnosticsModal.cpp"
        "src/ui/modals/AboutModal.cpp"
        "src/ui/modals/C6FlasherModal.cpp"
        "src/audio/AudioPlayer.cpp"
        "src/audio/RadioManager.cpp"

        "src/tlv/mesh_header.cpp"
        "src/tlv/tlv_dictionary.cpp"
        "src/tlv/tlv_parser.cpp"
        "src/mesh/MeshEngine.cpp"

        "src/ui/views/LuappView.cpp"
        "src/network/ConfigManager.cpp"
        "src/network/http_client.cpp"
        "src/time/TimeManager.cpp"
        "src/lua/LuaEngine.cpp"
        "src/lua/LuaBridge.cpp"
        "src/lua/LuaRunner.cpp"
        "src/lua/LuappManager.cpp"
        ${HELIX_SRCS}
        ${LUA_SRCS}
    INCLUDE_DIRS 
        "include"
        "src/ui"
        "src/ui/views"
        "src/ui/views/utilities"
        "src/tlv"
        "src/mesh"
        "src/cartridge"
        "src/audio"
        "src/audio/libhelix/src"
        "src/audio/libhelix/src/utils"
        "src/audio/libhelix/src/libhelix-mp3"
        "src/audio/libhelix/src/libhelix-aac"
        "src/network"
        "src/time"
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
        "app_update"
        "esp_partition"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-maybe-uninitialized -Wno-unused-but-set-variable -Wno-unused-variable)

target_compile_definitions(${COMPONENT_LIB} PUBLIC -DESP32 -DUSE_DEFAULT_STDLIB -DHELIX_LOGGING_ACTIVE=0 -DLUA_32BITS)
target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-error -Wno-format -Wno-unused-variable)

