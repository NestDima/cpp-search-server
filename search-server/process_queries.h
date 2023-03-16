#pragma once
#include "search_server.h"
#include <vector>
#include <string>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries);
