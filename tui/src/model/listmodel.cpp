#include "listmodel.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace model {

namespace {

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // namespace

SortSpec nextOnHeaderClick(SortSpec current, int column)
{
    if (current.column != column)
        return {column, true};
    if (current.ascending)
        return {column, false};
    return {-1, true};
}

std::string category(const octo::Package &p)
{
    return p.tap;
}

std::vector<std::pair<std::string, int>> categories(const std::vector<octo::Package> &all)
{
    std::map<std::string, int> count;
    for (const auto &p : all) {
        const std::string c = category(p);
        if (!c.empty())
            ++count[c];
    }
    return {count.begin(), count.end()};
}

bool isLeaf(const octo::Package &p)
{
    return p.installed && p.kind == octo::Kind::Formula && p.requiredBy == 0;
}

std::vector<int> visibleRows(const std::vector<octo::Package> &all, const std::string &filter, SortSpec sort,
                             SearchMode mode, const std::string &cat, bool leavesOnly)
{
    const std::string needle = lower(filter);
    std::vector<int> rows;
    std::vector<std::string> keys(all.size());   // nomes em minúsculas, calculados uma vez
    for (size_t i = 0; i < all.size(); ++i) {
        keys[i] = lower(all[i].name);
        if (!cat.empty() && category(all[i]) != cat)
            continue;
        if (leavesOnly && !isLeaf(all[i]))
            continue;
        bool match = needle.empty() || keys[i].find(needle) != std::string::npos;
        if (!match && mode == SearchMode::Description)
            match = lower(all[i].comment).find(needle) != std::string::npos;
        if (match)
            rows.push_back(static_cast<int>(i));
    }
    if (sort.column < 0 || sort.column >= ColumnCount)
        return rows;

    const auto less = [&](int x, int y) {
        const octo::Package &a = all[static_cast<size_t>(x)], &b = all[static_cast<size_t>(y)];
        switch (sort.column) {
        case StateIcon:   return a.status < b.status;
        case Name:        return keys[static_cast<size_t>(x)] < keys[static_cast<size_t>(y)];
        case Type:        return a.kind < b.kind;
        case Version:     return octo::compareVersions(a.version, b.version) < 0;
        case Size:        return a.size < b.size;
        case InstalledOn: return a.installedOn < b.installedOn;
        default:          return false;
        }
    };
    std::stable_sort(rows.begin(), rows.end(),
                     [&](int x, int y) { return sort.ascending ? less(x, y) : less(y, x); });
    return rows;
}

int rowToSelect(const std::vector<octo::Package> &all, const std::vector<int> &rows, const std::string &previousKey)
{
    if (rows.empty())
        return -1;
    for (size_t i = 0; i < rows.size(); ++i)
        if (all[static_cast<size_t>(rows[i])].key() == previousKey)
            return static_cast<int>(i);
    return 0;
}

int scrollOffset(int selected, int offset, int height, int count)
{
    if (height <= 0 || count <= 0)
        return 0;
    if (selected < 0)
        selected = 0;
    if (selected < offset)
        offset = selected;
    else if (selected >= offset + height)
        offset = selected - height + 1;
    return std::max(0, std::min(offset, std::max(0, count - height)));
}

} // namespace model
