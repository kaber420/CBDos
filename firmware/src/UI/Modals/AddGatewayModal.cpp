#include "AddGatewayModal.h"
#include "../UIManager.h"
#include "../Themes/DefaultTheme.h"
#include "../../Network/ConfigManager.h"
#ifdef ARDUINO
#include <SD.h>
#endif

lv_obj_t* AddGatewayModal::maskObj = nullptr;
lv_obj_t* AddGatewayModal::taPin = nullptr;
lv_obj_t* AddGatewayModal::dropdownFiles = nullptr;
std::vector<std::string> AddGatewayModal::encFiles;

void AddGatewayModal::cancel_cb(lv_event_t* e) {
    if (maskObj && lv_obj_is_valid(maskObj)) {
        lv_obj_delete_async(maskObj);
        maskObj = nullptr;
    }
}

void AddGatewayModal::import_cb(lv_event_t* e) {
    if (!dropdownFiles || !taPin) return;

    char selectedFile[128] = "";
    lv_dropdown_get_selected_str(dropdownFiles, selectedFile, sizeof(selectedFile));

    const char* pinText = lv_textarea_get_text(taPin);
    if (!pinText || strlen(pinText) == 0) {
        UIManager::showToast("Ingrese el PIN");
        return;
    }

#ifdef ARDUINO
    String encPath = String("/gateways/") + String(selectedFile);
    if (!SD.exists(encPath)) {
        encPath = String("/") + String(selectedFile);
    }
    String errorOut;
    if (ConfigManager::getInstance().importGateway(encPath, String(pinText), errorOut)) {
        UIManager::showToast("Gateway importado con éxito");
        if (maskObj && lv_obj_is_valid(maskObj)) {
            lv_obj_delete_async(maskObj);
            maskObj = nullptr;
        }
        UIManager::getInstance().loadGatewayConfig();
    } else {
        UIManager::showToast(errorOut.c_str());
    }
#else
    std::string encPath = "/" + std::string(selectedFile);
    std::string errorOut;
    if (ConfigManager::getInstance().importGateway(encPath, std::string(pinText), errorOut)) {
        UIManager::showToast("Gateway Mock importado");
        if (maskObj && lv_obj_is_valid(maskObj)) {
            lv_obj_delete_async(maskObj);
            maskObj = nullptr;
        }
        UIManager::getInstance().loadGatewayConfig();
    } else {
        UIManager::showToast(errorOut.c_str());
    }
#endif
}

void AddGatewayModal::show(lv_obj_t* parent) {
    maskObj = lv_obj_create(lv_layer_top());
    lv_obj_set_size(maskObj, 320, 480);
    lv_obj_set_pos(maskObj, 0, 0);
    lv_obj_set_style_bg_color(maskObj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(maskObj, LV_OPA_80, 0);
    lv_obj_set_style_border_width(maskObj, 0, 0);

    lv_obj_t* modal = lv_obj_create(maskObj);
    lv_obj_set_width(modal, 280);
    lv_obj_set_height(modal, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(modal, 20);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 16, 0);
    lv_obj_set_style_pad_row(modal, 12, 0);

    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, "Importar Gateway (.enc)");
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    // Buscar archivos .enc
    encFiles.clear();
    std::string optionsStr = "";

#ifdef ARDUINO
    if (SD.exists("/gateways")) {
        File dir = SD.open("/gateways");
        if (dir && dir.isDirectory()) {
            File file = dir.openNextFile();
            while (file) {
                String fname = String(file.name());
                if (fname.endsWith(".enc")) {
                    encFiles.push_back(fname.c_str());
                    if (optionsStr.length() > 0) optionsStr += "\n";
                    optionsStr += fname.c_str();
                }
                file.close();
                file = dir.openNextFile();
            }
            dir.close();
        }
    }
    if (encFiles.empty() && SD.exists("/")) {
        File root = SD.open("/");
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                String fname = String(file.name());
                if (fname.endsWith(".enc")) {
                    encFiles.push_back(fname.c_str());
                    if (optionsStr.length() > 0) optionsStr += "\n";
                    optionsStr += fname.c_str();
                }
                file.close();
                file = root.openNextFile();
            }
            root.close();
        }
    }
#endif

    if (optionsStr.empty()) {
        optionsStr = "gw_nuevo.enc";
    }

    lv_obj_t* lblFile = lv_label_create(modal);
    lv_label_set_text(lblFile, "Archivo en SD:");
    lv_obj_set_style_text_color(lblFile, DefaultTheme::getMutedTextColor(), 0);

    dropdownFiles = lv_dropdown_create(modal);
    lv_obj_set_width(dropdownFiles, lv_pct(100));
    lv_dropdown_set_options(dropdownFiles, optionsStr.c_str());
    DefaultTheme::applyRaisedCard(dropdownFiles, 10);

    lv_obj_t* lblPin = lv_label_create(modal);
    lv_label_set_text(lblPin, "PIN de Descifrado:");
    lv_obj_set_style_text_color(lblPin, DefaultTheme::getMutedTextColor(), 0);

    taPin = lv_textarea_create(modal);
    lv_obj_set_width(taPin, lv_pct(100));
    lv_textarea_set_one_line(taPin, true);
    lv_textarea_set_password_mode(taPin, true);
    DefaultTheme::applyRaisedCard(taPin, 10);
    UIManager::attachKeyboard(taPin);

    // Contenedor de botones
    lv_obj_t* btnCont = lv_obj_create(modal);
    lv_obj_set_width(btnCont, lv_pct(100));
    lv_obj_set_height(btnCont, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btnCont, 0, 0);
    lv_obj_set_style_border_width(btnCont, 0, 0);
    lv_obj_set_flex_flow(btnCont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnCont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btnCont, 0, 0);

    lv_obj_t* btnCancel = lv_button_create(btnCont);
    lv_obj_set_size(btnCancel, 110, 40);
    DefaultTheme::applyButton(btnCancel, 12);
    lv_obj_t* lblC = lv_label_create(btnCancel);
    lv_label_set_text(lblC, "Cancelar");
    lv_obj_set_style_text_color(lblC, DefaultTheme::getTextColor(), 0);
    lv_obj_center(lblC);
    lv_obj_add_event_cb(btnCancel, cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btnImport = lv_button_create(btnCont);
    lv_obj_set_size(btnImport, 110, 40);
    DefaultTheme::applyButton(btnImport, 12);
    lv_obj_set_style_bg_color(btnImport, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_t* lblI = lv_label_create(btnImport);
    lv_label_set_text(lblI, "Importar");
    lv_obj_set_style_text_color(lblI, lv_color_hex(0x0F172A), 0);
    lv_obj_center(lblI);
    lv_obj_add_event_cb(btnImport, import_cb, LV_EVENT_CLICKED, NULL);
}
