#include "DataParser.h"

bool DataParser::isValid(const std::string &line) const {
    return line.find("\"CAN_ID\"") != std::string::npos &&
           line.find("\"Data\"")   != std::string::npos;
}
