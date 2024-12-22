#include <SFML/Graphics.hpp>
#include <map>
#include <string>

extern "C" {

// 管理視窗和文字物件的全域變數
std::map<void*, sf::RenderWindow*> windows;
std::map<void*, sf::Text*> texts;
std::map<void*, sf::Font*> fonts;

// 創建視窗
void* create_window(int width, int height, const char* title) {
    sf::RenderWindow* window = new sf::RenderWindow(sf::VideoMode(width, height), title);
    windows[window] = window;
    return static_cast<void*>(window);
}

// 清除視窗
void clear_window(void* window, int r, int g, int b) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    win->clear(sf::Color(r, g, b));
}

// 顯示視窗內容
void display_window(void* window) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    win->display();
}

// 關閉視窗
void close_window(void* window) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    windows.erase(window);
    delete win;
}

// 創建標題文字
void* create_text(void* window, const char* font_path, const char* text, int size, int x, int y, int r, int g, int b) {
    sf::Font* font = new sf::Font();
    if (!font->loadFromFile(font_path)) {
        delete font;
        return nullptr;
    }
    sf::Text* txt = new sf::Text();
    txt->setFont(*font);
    txt->setString(text);
    txt->setCharacterSize(size);
    txt->setPosition(x, y);
    txt->setFillColor(sf::Color(r, g, b));

    fonts[txt] = font;
    texts[txt] = txt;
    return static_cast<void*>(txt);
}

// 繪製文字
void draw_text(void* window, void* text) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    sf::Text* txt = static_cast<sf::Text*>(text);
    win->draw(*txt);
}

// 刪除文字
void delete_text(void* text) {
    sf::Text* txt = static_cast<sf::Text*>(text);
    sf::Font* font = fonts[txt];
    texts.erase(txt);
    fonts.erase(txt);
    delete txt;
    delete font;
}

}
