#include <SFML/Graphics.hpp>
#include <map>
#include <string>

extern "C" {

// 管理視窗和文字物件的全域變數
std::map<void*, sf::RenderWindow*> windows;
std::map<void*, sf::Text*> texts;
std::map<void*, sf::Font*> fonts;
// 存儲紋理與精靈的全局變數
std::map<void*, sf::Texture*> textures;
std::map<void*, sf::Sprite*> sprites;
// 使用全局容器管理多個按鈕（方格）
std::map<void*, sf::RectangleShape*> buttons;

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

void poll_events(void* window) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    sf::Event event;
    while (win->pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            win->close();
        }
    }
}



// 加載圖片並創建 Sprite
void* load_image(const char* image_path) {
    sf::Texture* texture = new sf::Texture();
    if (!texture->loadFromFile(image_path)) {
        delete texture;
        return nullptr;
    }

    sf::Sprite* sprite = new sf::Sprite();
    sprite->setTexture(*texture);

    // 儲存到全局變數
    textures[sprite] = texture;
    sprites[sprite] = sprite;

    return static_cast<void*>(sprite);
}

// 繪製圖片
void draw_image(void* window, void* image, int x, int y) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    sf::Sprite* sprite = static_cast<sf::Sprite*>(image);
    sprite->setPosition(x, y);
    win->draw(*sprite);
}

// 刪除圖片資源
void delete_image(void* image) {
    sf::Sprite* sprite = static_cast<sf::Sprite*>(image);
    sf::Texture* texture = textures[sprite];

    textures.erase(sprite);
    sprites.erase(sprite);

    delete sprite;
    delete texture;
}

// 繪製矩形
void draw_rectangle(void* window, int x, int y, int width, int height, int r, int g, int b) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    sf::RectangleShape rectangle(sf::Vector2f(width, height));
    rectangle.setPosition(x, y);
    rectangle.setFillColor(sf::Color(r, g, b));
    win->draw(rectangle);
}

// 創建按鈕
void* create_button(int x, int y, int width, int height, int r, int g, int b) {
    sf::RectangleShape* button = new sf::RectangleShape(sf::Vector2f(width, height));
    button->setPosition(x, y);
    button->setFillColor(sf::Color(r, g, b));
    buttons[button] = button;
    return static_cast<void*>(button);
}

// 繪製按鈕
void draw_button(void* window, void* button) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    sf::RectangleShape* btn = static_cast<sf::RectangleShape*>(button);
    win->draw(*btn);
}

void* detect_clicked_button(void* window, int mouse_x, int mouse_y) {
    for (auto& [key, button] : buttons) {
        if (button->getGlobalBounds().contains(mouse_x, mouse_y)) {
            return key; // 返回被點擊的按鈕
        }
    }
    return nullptr; // 沒有按鈕被點擊
}

// 檢查按鈕點擊
int is_button_clicked(void* window, void* button, int mouse_x, int mouse_y) {
    sf::RectangleShape* btn = static_cast<sf::RectangleShape*>(button);
    sf::FloatRect bounds = btn->getGlobalBounds();
    return bounds.contains(mouse_x, mouse_y);
}

// 刪除按鈕
void delete_button(void* button) {
    auto it = buttons.find(button);
    if (it != buttons.end()) {
        delete it->second;
        buttons.erase(it);
    }
}

// 獲取滑鼠的 X 座標
int get_mouse_position_x(void* window) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    sf::Vector2i position = sf::Mouse::getPosition(*win);
    return position.x;
}

// 獲取滑鼠的 Y 座標
int get_mouse_position_y(void* window) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    sf::Vector2i position = sf::Mouse::getPosition(*win);
    return position.y;
}

// 檢測滑鼠左鍵是否被按下
int is_mouse_button_pressed() {
    return sf::Mouse::isButtonPressed(sf::Mouse::Left) ? 1 : 0;
}

}
