#include <string>
#include <cctype>
#include <vector>

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

std::vector<std::string> tokenizePath(const std::string &path)
{
    std::vector<std::string> tokens;
    std::string component;
    for (size_t i = 0; i <= path.size(); i++)
    {
        if (i == path.size() || path[i] == '/')
        {
            if (!component.empty() && component != ".")
                tokens.push_back(component);
            component.clear();
        }
        else
        {
            component += path[i];
        }
    }
    return tokens;
}