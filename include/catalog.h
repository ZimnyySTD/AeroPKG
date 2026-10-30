#pragma once

#include "common.h"
#include <vector>
#include <string>

class CatalogEngine {
public:
    CatalogEngine();
    ~CatalogEngine();

    bool loadCatalog();
    bool loadFromUrl(const std::string& repoUrl);
    bool loadFromFile(const std::string& jsonFilePath);

    // Filter & Search
    std::vector<CatalogItem> search(const std::string& query, const std::string& regionFilter = "");
    const std::vector<CatalogItem>& getAllItems() const { return items; }

    // Repository configuration
    void addRepository(const std::string& url);
    const std::vector<std::string>& getRepositories() const { return repositoryUrls; }

private:
    std::vector<CatalogItem> items;
    std::vector<std::string> repositoryUrls;

    bool parseJsonString(const std::string& jsonStr);
};
