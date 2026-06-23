#include <string>
#include <cctype>

std::string trim(const std::string& str) {
    size_t inicio = 0;
    while (inicio < str.size() &&
           std::isspace(static_cast<unsigned char>(str[inicio]))) {
        inicio++;
    }

    size_t fim = str.size();
    while (fim > inicio &&
           std::isspace(static_cast<unsigned char>(str[fim - 1]))) {
        fim--;
    }

    return str.substr(inicio, fim - inicio);
}