//
// Created by cpasjuste on 14/06/23.
//

#ifndef MICROBOY_UI_H
#define MICROBOY_UI_H

#include <vector>

#define UI_FONT_HEIGHT 16

namespace mb {
    class Ui {
    public:
        enum Color {
            Red = 0xC083,
            Yellow = 0xF5C1,
            Green = 0x0388,
            Blue = 0x01CF,
            Gray = 0x52CB,
            GrayDark = 0x2986
        };

        explicit Ui(Platform *platform);

        bool loop();

    private:
        Platform *p_platform;
        std::vector<std::string> m_files;
        int m_max_lines = 0;
        int m_line_height = 0;
        int m_file_index = 0;
        int m_highlight_index = 0;

        void setSelection(int index);

        void flip();
    };
}

#endif //MICROBOY_UI_H