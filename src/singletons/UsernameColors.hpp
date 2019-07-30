#pragma once

#include <map>

namespace chatterino {
class UsernameColors
{
public:
    static UsernameColors &getInstance();
    QColor getColor(const int userId);

    UsernameColors(const UsernameColors &) = delete;

private:
	UsernameColors();

    std::map<int, QColor> usernameRandomColors_;
};
}  // namespace chatterino
