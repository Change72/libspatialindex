/******************************************************************************
 * Project:  libspatialindex - A C++ library for spatial indexing
 * Author:   Marios Hadjieleftheriou, mhadji@gmail.com
 ******************************************************************************
 * Copyright (c) 2002, Marios Hadjieleftheriou
 *
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
******************************************************************************/

// NOTE: Please read README.txt before browsing this code.

#include <cstring>

// include library header file.
#include <spatialindex/SpatialIndex.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <regex>

using namespace SpatialIndex;
using namespace std;

#define INSERT 1
#define DELETE 0
#define QUERY 2

// example of a Visitor pattern.
// findes the index and leaf IO for answering the query and prints
// the resulting data IDs to stdout.
class MyVisitor : public IVisitor
{
public:
    size_t m_indexIO{0};
    size_t m_leafIO{0};

public:
    MyVisitor() = default;

    vector<const IShape*> results;  // Store overlapping regions

    void visitNode(const INode& n) override
    {
        if (n.isLeaf()) m_leafIO++;
        else m_indexIO++;
    }

    // Called when visiting data (leaf entries)
    void visitData(const IData& d) override {
        IShape* shape = nullptr;
        d.getShape(&shape);  // Get the region (shape) of the data entry
        results.push_back(shape);  // Store overlapping region

        // Print the region's start (low) and end (high) coordinates
        const Region* region = dynamic_cast<const Region*>(shape);
//        if (region) {
//            cout << "Overlapping Region: Start (";
//            for (size_t i = 0; i < region->getDimension(); ++i) {
//                cout << region->m_pLow[i];
//                if (i < region->getDimension() - 1) cout << ", ";
//            }
//
//            cout << ") End (";
//            for (size_t i = 0; i < region->getDimension(); ++i) {
//                cout << region->m_pHigh[i];
//                if (i < region->getDimension() - 1) cout << ", ";
//            }
//            cout << ")" << endl;
//        }
    }

    void visitData(std::vector<const IData*>& v) override {
        for (const IData* data : v) {
            visitData(*data);
        }
    }
};

// example of a Strategy pattern.
// traverses the tree by level.
class MyQueryStrategy : public SpatialIndex::IQueryStrategy
{
private:
    queue<id_type> ids;

public:
    void getNextEntry(const IEntry& entry, id_type& nextEntry, bool& hasNext) override
    {
        IShape* ps;
        entry.getShape(&ps);
        Region* pr = dynamic_cast<Region*>(ps);

        cout << pr->m_pLow[0] << " " << pr->m_pLow[1] << endl;
        cout << pr->m_pHigh[0] << " " << pr->m_pLow[1] << endl;
        cout << pr->m_pHigh[0] << " " << pr->m_pHigh[1] << endl;
        cout << pr->m_pLow[0] << " " << pr->m_pHigh[1] << endl;
        cout << pr->m_pLow[0] << " " << pr->m_pLow[1] << endl << endl << endl;
        // print node MBRs gnuplot style!

        delete ps;

        const INode* n = dynamic_cast<const INode*>(&entry);

        // traverse only index nodes at levels 2 and higher.
        if (n != nullptr && n->getLevel() > 1)
        {
            for (uint32_t cChild = 0; cChild < n->getChildrenCount(); cChild++)
            {
                ids.push(n->getChildIdentifier(cChild));
            }
        }

        if (! ids.empty())
        {
            nextEntry = ids.front(); ids.pop();
            hasNext = true;
        }
        else
        {
            hasNext = false;
        }
    }
};


// Function to parse a tuple string and extract integer values
vector<int> parseTuple(const string& tupleStr) {
    vector<int> values;
    regex numRegex(R"(\d+)");
    sregex_iterator it(tupleStr.begin(), tupleStr.end(), numRegex);
    sregex_iterator end;
    while (it != end) {
        values.push_back(stoi(it->str()));
        ++it;
    }
    return values;
}


int main(int argc, char** argv)
{
    try
    {
        if (argc != 4)
        {
            cerr << "Usage: " << argv[0] << " query_file tree_capacity query_type [intersection | 10NN | selfjoin]." << endl;
            return -1;
        }

        uint32_t queryType = 0;

        if (strcmp(argv[3], "intersection") == 0) queryType = 0;
        else if (strcmp(argv[3], "10NN") == 0) queryType = 1;
        else if (strcmp(argv[3], "selfjoin") == 0) queryType = 2;
        else
        {
            cerr << "Unknown query type." << endl;
            return -1;
        }

        ifstream fin(argv[1]);
        if (! fin)
        {
            cerr << "Cannot open query file " << argv[1] << "." << endl;
            return -1;
        }

        // string baseName = argv[2];
        // IStorageManager* diskfile = StorageManager::loadDiskStorageManager(baseName);
        // this will try to locate and open an already existing storage manager.

        // StorageManager::IBuffer* file = StorageManager::createNewRandomEvictionsBuffer(*diskfile, 10, false);
        // applies a main memory random buffer on top of the persistent storage manager
        // (LRU buffer, etc can be created the same way).

        // If we need to open an existing tree stored in the storage manager, we only
        // have to specify the index identifier as follows
        // ISpatialIndex* tree = RTree::loadRTree(*file, 1);
        id_type indexIdentifier;
        IStorageManager* memoryFile = StorageManager::createNewMemoryStorageManager();
        ISpatialIndex* tree = RTree::createNewRTree(*memoryFile, 0.7, atoi(argv[2]), atoi(argv[2]), 3, SpatialIndex::RTree::RV_RSTAR, indexIdentifier);

        id_type id;
        int64_t id_count = 0;
        string line;
        vector<pair<vector<int>, vector<int>>> queries;

        // Skip the first line (header)
        getline(fin, line);
        regex tupleRegex(R"(\"?\(([^)]+)\)\"?,? ?\"?\(([^)]+)\)\"?)");
        smatch match;

        while (getline(fin, line)) {
            stringstream ss(line);
            regex_search(line, match, tupleRegex);

            string startStr = match[1].str();  // First tuple content
            string countStr = match[2].str();  // Second tuple content

            vector<int> startValues = parseTuple(startStr);
            vector<int> countValues = parseTuple(countStr);

            queries.emplace_back(startValues, countValues);
        }

        // Insert all the queries into the tree
        for (const auto& query : queries) {
            id_count++;

            vector<int> startValues = query.first;
            vector<int> countValues = query.second;

            double low[3], high[3];
            for (int i = 0; i < 3; i++) {
                low[i] = startValues[i];
                high[i] = startValues[i] + countValues[i];
            }

            Region r = Region(low, high, 3);
            tree->insertData(0, nullptr, r, id_count);
        }

        std::cerr << "Operations: " << id_count << std::endl;
        std::cerr << *tree;

        bool ret = tree->isIndexValid();
        if (ret == false) std::cerr << "ERROR: Structure is invalid!" << std::endl;
        else std::cerr << "The structure seems O.K." << std::endl;

        // query part
        for (const auto& query : queries) {
            vector<int> startValues = query.first;
            vector<int> countValues = query.second;

            double low[3], high[3];
            for (int i = 0; i < 3; i++) {
                low[i] = startValues[i];
                high[i] = startValues[i] + countValues[i];
            }

            MyVisitor vis;

            Region r = Region(low, high, 3);
            tree->intersectsWithQuery(r, vis);

            // Print the current query
            cout << "Query: Start (";
            for (size_t i = 0; i < r.getDimension(); ++i) {
                cout << r.m_pLow[i];
                if (i < r.getDimension() - 1) cout << ", ";
            }
            cout << ") End (";
            for (size_t i = 0; i < r.getDimension(); ++i) {
                cout << r.m_pHigh[i];
                if (i < r.getDimension() - 1) cout << ", ";
            }
            cout << ")" << endl;

            // Print all the overlapping regions
            for (const IShape* shape : vis.results) {
                const Region* region = dynamic_cast<const Region*>(shape);
                if (region) {
                    cout << "Overlapping Region: Start (";
                    for (size_t i = 0; i < region->getDimension(); ++i) {
                        cout << region->m_pLow[i];
                        if (i < region->getDimension() - 1) cout << ", ";
                    }

                    cout << ") End (";
                    for (size_t i = 0; i < region->getDimension(); ++i) {
                        cout << region->m_pHigh[i];
                        if (i < region->getDimension() - 1) cout << ", ";
                    }
                    cout << ") ----------    ";
                }
            }
            cout << endl;
        }

        delete tree;
    }
    catch (Tools::Exception& e)
    {
        cerr << "******ERROR******" << endl;
        std::string s = e.what();
        cerr << s << endl;
        return -1;
    }
    catch (...)
    {
        cerr << "******ERROR******" << endl;
        cerr << "other exception" << endl;
        return -1;
    }

    return 0;
}
