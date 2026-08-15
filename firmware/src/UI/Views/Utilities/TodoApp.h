#pragma once

#include <lvgl.h>
#include <string>
#include <vector>

struct TodoItem {
    std::string text;
    bool done;
};

struct TodoList {
    std::string name;
    std::vector<TodoItem> items;
};

class TodoApp {
public:
    static void build(lv_obj_t* parent);
    static void cleanup();

    // Acceso al modelo de datos para extensiones futuras
    static const std::vector<TodoList>& getLists();
    static void addList(const std::string& name);
    static void deleteList(size_t index);

private:
    static void loadFromStorage();
    static void saveToStorage();
    static void refreshListUI();

    static void checkboxEventCb(lv_event_t* e);
    static void deleteItemCb(lv_event_t* e);
    static void addBtnCb(lv_event_t* e);
    static void clearCompletedCb(lv_event_t* e);
    static void dropdownCb(lv_event_t* e);
};
