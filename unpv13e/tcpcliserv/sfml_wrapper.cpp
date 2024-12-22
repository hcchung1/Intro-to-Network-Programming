#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

// 將函數暴露給 C 語言使用
extern "C" {

// 初始化視窗
void* create_window(int width, int height, const char* title) {
    sf::RenderWindow* window = new sf::RenderWindow(sf::VideoMode(width, height), title);
    return static_cast<void*>(window);
}

// 清除視窗
void clear_window(void* window, int r, int g, int b) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    win->clear(sf::Color(r, g, b));
}

// 顯示內容
void display_window(void* window) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    win->display();
}

// 關閉視窗
void close_window(void* window) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    win->close();
    delete win;
}

// 檢查視窗是否開啟
int is_window_open(void* window) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    return win->isOpen() ? 1 : 0;
}

// 處理事件
int poll_event(void* window) {
    sf::RenderWindow* win = static_cast<sf::RenderWindow*>(window);
    sf::Event event;
    while (win->pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            win->close();
            return 1; // 表示關閉事件
        }
    }
    return 0; // 沒有關閉事件
}

}
