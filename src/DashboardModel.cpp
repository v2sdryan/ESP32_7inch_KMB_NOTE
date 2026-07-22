#include "DashboardModel.h"

#include <algorithm>

namespace DashboardModel
{
static std::string firstUtf8Characters(const std::string &text,
                                       std::size_t maxCharacters)
{
    std::size_t offset = 0;
    std::size_t count = 0;
    while (offset < text.size() && count < maxCharacters) {
        const unsigned char lead = static_cast<unsigned char>(text[offset]);
        std::size_t length = 1;
        if ((lead & 0xE0U) == 0xC0U) length = 2;
        else if ((lead & 0xF0U) == 0xE0U) length = 3;
        else if ((lead & 0xF8U) == 0xF0U) length = 4;
        if (offset + length > text.size()) break;
        offset += length;
        ++count;
    }
    return text.substr(0, offset);
}

std::string formatBusEta(const DashboardBusItem &item)
{
    const std::string first = item.eta1.empty() ? "-" : item.eta1;
    const std::string second = item.eta2.empty() ? "-" : item.eta2;
    const std::string destination = firstUtf8Characters(item.destination, 4);
    return item.route + (destination.empty() ? " " : " " + destination + " ") +
           first + " / " + second + " 分";
}

std::vector<DashboardBusItem> visibleBuses(
    const std::vector<DashboardBusItem> &items)
{
    const std::size_t count = std::min(items.size(), kMaxVisibleBuses);
    return {items.begin(), items.begin() + static_cast<std::ptrdiff_t>(count)};
}

const char *emptyBusMessage()
{
    return "尚未設定巴士收藏";
}

std::string sanitizeForDisplay(const std::string &text)
{
    std::string output;
    for (std::size_t offset = 0; offset < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[offset]);
        uint32_t codepoint = lead;
        std::size_t length = 1;
        if ((lead & 0xE0U) == 0xC0U && offset + 1 < text.size()) {
            codepoint = ((lead & 0x1FU) << 6U) |
                        (static_cast<unsigned char>(text[offset + 1]) & 0x3FU);
            length = 2;
        } else if ((lead & 0xF0U) == 0xE0U && offset + 2 < text.size()) {
            codepoint = ((lead & 0x0FU) << 12U) |
                        ((static_cast<unsigned char>(text[offset + 1]) & 0x3FU) << 6U) |
                        (static_cast<unsigned char>(text[offset + 2]) & 0x3FU);
            length = 3;
        } else if ((lead & 0xF8U) == 0xF0U && offset + 3 < text.size()) {
            codepoint = ((lead & 0x07U) << 18U) |
                        ((static_cast<unsigned char>(text[offset + 1]) & 0x3FU) << 12U) |
                        ((static_cast<unsigned char>(text[offset + 2]) & 0x3FU) << 6U) |
                        (static_cast<unsigned char>(text[offset + 3]) & 0x3FU);
            length = 4;
        }

        if (codepoint == '\n') {
            output += '\n';
        } else if ((codepoint >= 0x20U && codepoint <= 0x7EU) ||
            (codepoint >= 0x4E00U && codepoint <= 0x9FFFU)) {
            output.append(text, offset, length);
        } else if (codepoint == 0x3001U || codepoint == 0xFF0CU) {
            output += ", ";
        } else if (codepoint == 0x3002U) {
            output += '.';
        } else if (codepoint == 0xFF1AU) {
            output += ':';
        } else if (codepoint == 0xFF1BU) {
            output += ';';
        } else if (codepoint == 0xFF08U) {
            output += '(';
        } else if (codepoint == 0xFF09U) {
            output += ')';
        } else if (codepoint == 0xFF0FU) {
            output += '/';
        } else if (codepoint == 0x3000U || codepoint > 0xFFFFU) {
            output += ' ';
        }
        offset += length;
    }
    return output;
}
}
