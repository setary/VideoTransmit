#include <string>

bool dig_hole(const std::string& client_name);
bool fill_hole(const std::string& client_name);
bool fill_all_holes();
bool get_address_by_name(const std::string& client_name, std::string& ip, int& port);