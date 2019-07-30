#include "UsernameColors.hpp"

namespace chatterino {
UsernameColors &UsernameColors::getInstance()
{
    static UsernameColors *instance = new UsernameColors();
    return *instance;
}

UsernameColors::UsernameColors()
{

}

QColor UsernameColors::getColor(const int userId)
{
    static const std::vector<QColor> twitchUsernameColors{
        QColor(255, 0, 0), // Red
        QColor(0, 0, 255), // Blue
        QColor(0, 255, 0), // Green
        QColor(178, 34, 34), // FireBrick
        QColor(255, 127, 80), // Coral
        QColor(154, 205, 50), // YellowGreen
        QColor(255, 69, 0), // OrangeRed
        QColor(46, 139, 87), // SeaGreen
        QColor(218, 165, 32), // GoldenRod
        QColor(210, 105, 30), // Chocolate
        QColor(95, 158, 160), // CadetBlue
        QColor(30, 144, 255), // DodgerBlue
        QColor(255, 105, 180), // HotPink
        QColor(138, 43, 226), // BlueViolet
        QColor(0, 255, 127) // SpringGreen
    };

    auto it = this->usernameRandomColors_.find(userId);
    if (it == this->usernameRandomColors_.cend())
    {
        const auto color = twitchUsernameColors[
            std::rand() % twitchUsernameColors.size()];
        this->usernameRandomColors_.emplace(userId, color);
        return color;
    }
    else
    {
        return it->second;
    }
}

}  // namespace chatterino
