#ifndef TESTFILE_H
#define TESTFILE_H

class testFile{
public:
    string filename;
    string text;
    vector<string> words;
    vector<pair<string, string>> tags;
    map<pair<string, string>, string> collocations;
    enum enums::TOPIC topic;
    testFile* parent;
    vector<vector<pair<string,string>>*> nounPhrases;
    vector<vector<pair<string,string>>*> verbPhrases;
};


#endif // TESTFILE_H
