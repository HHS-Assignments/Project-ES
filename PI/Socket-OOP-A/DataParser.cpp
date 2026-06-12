#include "DataParser.h"
#include <cstdio>

bool DataParser::isTextId(uint32_t id) {
    return id == 0x150 || id == 0x160 || id == 0x170 ||
           id == 0x180 || id == 0x190 || id == 0x191;
}

bool DataParser::parseTijdConfig(const CanMessage &msg,
                                 int &dagH, int &dagM, int &nachtH, int &nachtM) {
    if (msg.len != 8) return false;
    dagH   = (msg.data[0] << 8) | msg.data[1];
    dagM   = (msg.data[2] << 8) | msg.data[3];
    nachtH = (msg.data[4] << 8) | msg.data[5];
    nachtM = (msg.data[6] << 8) | msg.data[7];
    return dagH < 24 && dagM < 60 && nachtH < 24 && nachtM < 60;
}

std::string DataParser::messageToJson(const CanMessage &msg) const {
    char hexId[12];
    std::snprintf(hexId, sizeof(hexId), "0x%03X", msg.id);

    char buf[256];
    if (isTextId(msg.id)) {
        // Alle databytes zijn ASCII-tekst; nul-padding beëindigt de C-string vanzelf
        char text[9] = {};
        int len = (msg.len > 8) ? 8 : (int)msg.len;
        for (int i = 0; i < len; i++) text[i] = (char)msg.data[i];
        std::snprintf(buf, sizeof(buf), "{\"CAN_ID\":\"%s\",\"Data\":\"%s\"}", hexId, text);
    } else {
        int val = (msg.len > 0) ? (int)msg.data[0] : 0;
        std::snprintf(buf, sizeof(buf), "{\"CAN_ID\":\"%s\",\"Data\":%d}", hexId, val);
    }
    return std::string(buf);
}

bool DataParser::jsonToMessage(const std::string &line, CanMessage &msg) const {
    // CAN_ID (hex string)
    size_t pId = line.find("\"CAN_ID\"");
    if (pId == std::string::npos) return false;
    size_t c = line.find(':', pId);
    if (c == std::string::npos) return false;
    size_t q1 = line.find('"', c);
    if (q1 == std::string::npos) return false;
    size_t q2 = line.find('"', q1 + 1);
    if (q2 == std::string::npos) return false;
    std::string idStr = line.substr(q1 + 1, q2 - q1 - 1);
    try {
        msg.id = (idStr.rfind("0x",0)==0 || idStr.rfind("0X",0)==0)
               ? (uint32_t)std::stoul(idStr, nullptr, 16)
               : (uint32_t)std::stoul(idStr, nullptr, 10);
    } catch (...) { return false; }

    // Data (integer; string-data wordt niet verwacht richting de bus)
    size_t pDt = line.find("\"Data\"");
    if (pDt == std::string::npos) return false;
    size_t c2 = line.find(':', pDt);
    if (c2 == std::string::npos) return false;
    ++c2;
    while (c2 < line.size() && line[c2] == ' ') ++c2;

    int dataVal = 0;
    if (c2 < line.size() && line[c2] != '"') {
        try { dataVal = std::stoi(line.substr(c2)); }
        catch (...) { return false; }
    }

    msg.data[0] = (uint8_t)(dataVal & 0xFF);
    msg.len     = 1;
    return true;
}
