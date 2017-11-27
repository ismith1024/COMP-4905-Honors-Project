#include "controller.h"
#include "QDebug"
#include "languageprocessor.h"
#include "repository.h"
#include "component.h"
#include "bayesianclassifier.h"
#include "tokenizer.h"
#include "QStringList"
#include "QString"
#include "unordered_set"
#include "topicanalyzer.h"
#include<string>

/******************************************************
 * Description of overall control flow:
 * Open a text case
 *   for each file:
 *       determine the topic                                                        --TODO: optimize
 *       scan for parents                                                           -- done
 *   for each line:
 *       tokenize (remove stop characters, split into words, fix capitalization)    -- done
 *       extract noun and verb phrases, put each into a collection                  -- done
 *       for each phrase:
 *           identify collocations based on augmented corpus                        -- TODO: tentatively works -- optimize parms, compile stop-words
 *               for each collocation:
 *                   associate with most likely material-article type in augmented corpus (subject to MIN_SUPPORT) (given the topic) --TODO: start
 *               for each remaining word:
 *                   associate with most likely material-article type in augmented corpus (subject to MIN_SUPPORT) (given the topic) --TODO: Bayesian not working
 *               for untagged words:
 *                   if candidate part number
 *                       associate with most likely material-article type based on bayesian classifier --done
 *                       associate with most likely suppleir based on Bayesian classifier --done
 *               for each association:
 *                   collapse based on:
 *                       deduplication                                               --TODO: start
 *                       taxonomy                                                    --TODO: start
 *       for each association:
 *           determine a parent based on probability of material-article type having the parent in:
 *                   the file                                                        --TODO: start
 *                   the parent file                                                 --TODO: start
 *
 * Output:
 * ||Filename||Line item||Parent||MFR||MPN||Description||Material-article type||
 *
 ******************************************************/

///////////////
/// \brief Controller::runTestCase
/// \param tcNum - the test case number
/// Expects a test case in the following text format:
/// <filename> word1 word2 ... wordn \n
/// One line per file
///
/// Syntax of a data file:
///
///  Tokens are delimited by spaces
///     The first token is the filename
///   Fragments are delmited by tabs (this is primarily used for spreadsheet cells)
///   Lines are terminated by a ~ Tilde character
///   Files are delimited by a newline character
///
/// To create a test file:
///     - Create one or more text files and/or tab-delimited tables.  Use the .txt extension.
///     - Use pdftotext (from the popplertools library) to convert a pdf to text
///         pdftotext source.pdf target.txt
///     - Write the filenames and file contents to a single text file:
///         more * | cat > out.txt
///     - Replace newline characters with the DOT symbol (in this system, the ~ character)
///         sed -i 's/\n/ ~ /g' out.txt
///     - If the source of the document is a pdf, a double newline is the DOT symbol.
///         sed -t 's/~~/|/g' out.txt
///         tr '~' ' ' < out.txt >out2.txt
///         tr '|' '~' < out2.txt >out3.txt
///         mv out3.txt out.txt
///     - Replace the more command's 14-colon symbol
///         sed -i 's/~::::::::::::::~/\n/g' out.txt
///     - Replace the newline that follows a filename with a space
///         sed -i 's/.txt\n/.txt /g' testcase2.txt
///         (or just do it by hand with a text editor)
///
///
void Controller::runTestCase(int tcNum){

    ///////////// Setup
    string dir = "/home/ian/Data/Testcases/";
    dir += to_string(tcNum) + "/testcase.txt";

    vector<TestFile*> files = vector<TestFile*>();
    vector<string> text = vector<string>();

    vector<Component*> results = vector<Component*>();
    BayesianClassifier bayes = BayesianClassifier();

    //Open the corpus
    processor.getXML();
    processor.countTags();

    //Train collocation classifier
    vector<pair<string,string>> colls = vector<pair<string,string>>();
    vector<Component*> components = vector<Component*>();
    repo.getComponentsIncludingGenerics(components);
    getCollocationsFromDBDescriptions(colls);
    //cout << "OBTAIN COLLOCATIONS:" << endl;
    //for(auto& entry: colls) cout << entry.first << ", " << entry.second << endl;

    //Get the supplier aliases
    map<string, int> supplierNumbers = map<string, int>();
    repo.getSupplierNumbers(supplierNumbers);

    ///////////// Open test case file
    vector<string>::iterator it;

    ifstream thefile(dir);
    string line = "";
    if (!thefile.is_open()){
        cout << "Cannot open " << dir << endl;
        return;
    }

    //else do the work
    //This part fills in the filename, <words collection>
    while ( getline (thefile,line) ) {

        TestFile* testFile = new TestFile();

        text = vector<string>();
        QString theLine = QString::fromStdString(line);

        //cout << "Read line: " << line << endl;

        QStringList pieces = theLine.split('~');

        testFile->filename = pieces.at(0).toStdString();
        pieces.pop_front();

        for(auto& entry: pieces){
            if(line.size() > 0)
            testFile->lines.push_back(entry.toStdString());
        }

        files.push_back(testFile);

    }
    thefile.close();

    for(auto& entry: files){
        cout << endl << "FILE: " << entry->filename << endl << ".............." << endl;
        for(auto& e2: entry->lines) cout << e2 << endl;
        cout << "....... " << endl;

        //obtain "phrases" :
        //  - for a table, a "phrase" is a table cell. These are all noun phrases.  A table is encoded as tab-delimited text.
        //  - for free text, noun and verb phrases are parsed by the LanguageProcessor module
        vector<string> allWords;

        for(auto& e2: entry->lines){

            string tab = "\t";
            if(UtilityAlgorithms::containsSubst(e2, tab)){

                QStringList pieces = QString::fromStdString(e2).split('\t');
                for(auto& e3: pieces){
                    QStringList words = e3.split(' ');
                    for(auto& i: words){
                        i = i.toLower();
                    }
                    vector<pair<string,string>>* phrase = new vector<pair<string,string>>();
                    for(auto& e4: words){
                        string st = e4.toLower().toStdString();
                        allWords.push_back(st);
                    }
                    //    void tag(vector<string>&, vector<pair<string, string>>&);
                    processor.tag(words, *phrase);
                    processor.applyTechDictionary(*phrase);

                    for(auto& i: *phrase){
                            if(UtilityAlgorithms::mapContainsKey(supplierNumbers, i.first)){
                            int n = supplierNumbers[i.first];
                            i.second = "NNP subtype=supp" + to_string(n);
                        }
                    }
///////////////////////////////////TODO: bayesian classifier on all untagged alphanumerics
/// 1.  Check the files to see if the number matches a filename
/// 2.  Use the bayesian classifier

                    cout << "Tagging phrase: ";
                    for(auto& i: words) cout << i.toStdString() << " ";
                    cout << "....." << endl;
                    for(auto& i: *phrase) cout << i.first << "-" << i.second << "...";
                    cout << endl << endl;
                    entry->nounPhrases.push_back(phrase);
                }

                //put all the words onto the words and tags collections


            } else {  // if it does not contain tabs
                //tokenize the line
                QStringList pieces = QString::fromStdString(e2).toLower().split(' ');
                for(auto& word: pieces) tok.removeStopCharacters(word);

                //store the words to be tagged
                for(auto& word: pieces){
                    if(word.size() > 0){
                        entry->words.push_back(word.toStdString());
                    }
                }


            } // end if contains tabs

        } // end for: e2 in lines

        //tag the words
        if(entry->words.size() != 0) processor.tag(entry->words, entry->tags);
        processor.applyTechDictionary(entry->tags);

        for(auto& i: entry->tags){
            cout << "Check Supplier: " << i.first << endl;
            if(UtilityAlgorithms::mapContainsKey(supplierNumbers, i.first)){
                int n = supplierNumbers[i.first];
                i.second = "NNP subtype=supp" + to_string(n);
            }
        }


        for(auto& i: entry->tags){
            cout << "(" << i.first << ", " << i.second << ") ";
        }

        cout << "END OF TAGS" << endl;

        //get the noun phrases
        processor.getNounPhrases(entry->tags, entry->nounPhrases);

        //get the verb phrases
        processor.getVerbPhrases(entry->tags, entry->verbPhrases);

        //put the orphan words in the word collection
        if(allWords.size() > 0){
            vector<pair<string, string>> tags = vector<pair<string, string>>();
            processor.tag(allWords, tags);
            for(auto& e3: allWords) entry->words.push_back(e3);
            for(auto& e3: tags) entry->tags.push_back(e3);
        }

        //search the phrases for collocations:
        //if found, remove the phrase and add to collocations
        //<vector<pair<string,string>>*>::iterator it;

        cout << "Checking collocations for file" << entry->filename << endl;

        for(auto& i: entry->nounPhrases){
            for(auto& j: colls){
                bool word1 = false;
                bool word2 = false;
                for(auto& k: *i){
                    if(k.first == j.first) word1 = true;
                    if(k.first == j.second) word2 = true;
                }
                if(word1 && word2){
                    entry->collocations[make_pair(j.first, j.second)] = "";
                }
            }
        }

        for(auto& i: entry->verbPhrases){
            for(auto& j: colls){
                bool word1 = false;
                bool word2 = false;
                for(auto& k: *i){
                    if(k.first == j.first) word1 = true;
                    if(k.first == j.second) word2 = true;
                }
                if(word1 && word2){
                     entry->collocations[make_pair(j.first, j.second)] = "";
                }
            }
        }

    } //end for: entry in files

    ///// Print the input files
    //for(auto& entry: files) cout << *entry;

    vector<Component*> finalResults = vector<Component*>();

    ////Create components.......................
    cout << "=====GENERATE COMPONENTS=====" << endl;
    for(auto& entry: files){
        cout << "Identify components from " << entry->filename << endl;
        for(auto& e2: entry->nounPhrases){
            buildComponentsFromPhrase(bayes, *e2, colls, components, finalResults);
        }
        for(auto& e2: entry->verbPhrases){
            buildComponentsFromPhrase(bayes, *e2, colls, components, finalResults);
        }
        //for(auto& entry: finalResults) cout << *entry << endl;
    }

    //vector<vector<pair<string,string>>*> nounPhrases

    consolidateCollection(finalResults);

    cout << " ----------------------" << endl;
    consolidateCollection(finalResults);

    cout << "FINAL RESULTS: " << endl;
    for(auto& entry: finalResults) cout << *entry << endl;;

    //shutdown
    for(auto& entry: files) delete entry;

}

///////////////
/// \brief Controller::buildComponentsFromPhrase
/// \return
/// Takes a phrase and builds Component objects from it
int Controller::buildComponentsFromPhrase(BayesianClassifier& bayes, vector<pair<string, string>>& tags, vector<pair<string,string>>& colls, vector<Component*>& compsIn, vector<Component*>& compsOut){

    vector<Component*> ret = vector<Component*>();

    string phrase = "";
    for(auto& entry: tags) phrase += " " + entry.first;
    cout << "Checking phrase: " + phrase;

    //check for collocation
    vector<pair<string,string>> keywords = vector<pair<string,string>>();
    repo.getTechKeywords(keywords);

    float MIN_BAYES_CONF = 0.001;

    for(auto& entry: tags){
        //check for a supplier
        //cout << "Check for supplier" << endl;
        /*string s = "NNP subtype=supp";
        if(UtilityAlgorithms::containsSubst(entry.second, s)){
            Component* c = new Component();
            c->mfr = entry.first;
            for(auto& str: tags){
                c->description += " " + str.first;
            }
            //classify based on description
            map<string, float>* results = bayes.classifyType(c->description, compsIn);
            pair<string, float> choice = UtilityAlgorithms::argmax(results);
            //cout << "Identified: " << choice.first << ".." << choice.second << endl;
            delete results;
            c->type = choice.first;
            ret.push_back(c);
        }*/
        //check for a part number
        //cout << "check for mpn" << endl;
        if(UtilityAlgorithms::isAlphanumeric(entry.first)){
            string cleanStr = entry.first;
            tok.removeStopCharacters(cleanStr);
            map<string, float>* results = bayes.classifyType(cleanStr, compsIn);
            pair<string, float> choice = UtilityAlgorithms::argmax(results);
            delete results;

            if(choice.second > MIN_BAYES_CONF){
                Component* c = new Component();
                c->mpn = entry.first;
                c->type = choice.first;
                string cleanMpn = entry.first;
                tok.removeStopCharacters(cleanMpn);
                //cout << "Old mpn: " << entry.first << " Clean: " << cleanMpn << endl;
                map<string, float>* res2 = bayes.classifySupplier(cleanMpn, compsIn);
                pair<string, float> mfr = UtilityAlgorithms::argmax(res2);

                cout << "Found mfr: " << mfr.first << endl;

                c->mfr = mfr.first;
                for(auto& str: tags){
                    c->description += " " + str.first;
                }
                ret.push_back(c);
                delete res2;
            }
        }
    }
    //check collocations

    //get the collocations from repository
    vector<pair<string,string>> foundColls = vector<pair<string,string>>();
    string first = "";
    string second = "";

    //cout << "Check colls" << endl;

    for(auto& e2: colls){
        first = "";
        second = "";
        for(auto& e3: tags){
            if(e3.first == e2.first && e3.first != "") first = e3.first;
            if(e3.first == e2.second && e3.second != "") second = e3.first;
        }
        if(first != "" && second != "") {
            foundColls.push_back(make_pair(first,second));
            //cout << "Collocation: <" << first << "," << second << ">" << endl;
        }
    }

    //identify the collocations
    if(foundColls.size() > 0){
        for(auto& entry: foundColls){
            cout << "Collocation: <" << entry.first << "," << entry.second << ">";
            Component* c = new Component();
            map<string, float>* results = bayes.classifyCollocation(entry, compsIn);

            if(results!= 0){
                pair<string, float> c1 = UtilityAlgorithms::argmax(results);

                string choice = c1.first;
                cout << " -- classified as: " << choice << endl;

                 if(results != 0) delete results;
                c->type = choice;
                c->mfr = "GENERIC";
                for(auto& str: tags){
                    c->description += " " + str.first;
                }
                c->mpn = c->description;
                ret.push_back(c);
            }
        }
    }
    //else, check technical keywords

    /*else*/{
        //cout << "Check keywords" << endl;
        for(auto& e2: keywords){

            for(auto& entry: tags){
                if(e2.first == entry.first){
                    Component* c = new Component();
                    c->type = e2.second;
                    c->mfr = "GENERIC";
                    for(auto& str: tags){
                        c->description += " " + str.first;
                    }
                    c->mpn = c->description;
                    ret.push_back(c);
                }
            }
        }
    }

    for(auto& entry: ret){
        //cout << *entry << "..";
        compsOut.push_back(entry);
    }

    cout << " ...complete" << endl;

}

void Controller::consolidateCollection(vector<Component*>& comps){
    //vector<Component*> cleanCollection = vector<Component*>();
    //vector<Component*> scrap = vector<Component*>();

    vector<Component*>::iterator it1, it2;

    for(it1 = comps.begin(); it1 < comps.end()-1; ++it1){
        for(it2 = it1+1; it2 < comps.end(); ++it2){

            if(**it1 == **it2){
                delete *it2;
                it2 = comps.erase(it2);
            } else {
                //remove a generic with the same type as an identified component
                if((*it1)->mfr == "GENERIC" && (*it1)->type == (*it2)->type){
                    if((*it2)->description  == "") (*it2)->description = (*it1)->description;
                    delete *it2;
                    it2 = comps.erase(it2);
                } else if((*it2)->mfr == "GENERIC" && (*it2)->type == (*it1)->type){
                    if((*it1)->description  == "") (*it1)->description = (*it2)->description;
                    delete *it1;
                    it1 = comps.erase(it1);
                } else if((*it2)->mfr == (*it1)->mfr && (*it2)->mpn == (*it1)->mpn){
                    delete *it2;
                    it2 = comps.erase(it2);
                } /*else if(                                                        ){

                }*/


                //end if components are similar


            } // end if components are equal

        } // end for it2
    } // end for it1


}


///////////////
/// \brief Controller::cleanTestCase
/// \param tcNum
/// Compiles the reuslts of the more * | cat to the format expected by runTestCase()
void Controller::cleanTestCase(int tcNum){
    /*
    string dir = "/home/ian/Data/Testcases/";
    dir += tcNum + "/testcase.txt";
    */
}


/////////
/// \brief Controller::getCollocationsFromDBDescriptions
/// Used during development to find the word collocations from the training set
void Controller::getCollocationsFromDBDescriptions(vector<pair<string, string>>& coll){

    vector<string> allDescriptions = vector<string>();
    map<string, int> singles = map<string, int>();
    map<pair<string,string>, int> pairs = map<pair<string,string>, int>();
    vector<pair<string,string>> collocations = vector<pair<string,string>>();

    //repo.getAllDescriptionsFromDB(allDescriptions);
    repo.getContractsComponentsDescriptionsFromDB(allDescriptions);

    //for(auto& entry: allDescriptions) cout << entry << endl;

    processor.findCollocationMetrics(allDescriptions, singles, pairs, tok);

    //for(auto& entry: singles) cout << entry.first << endl;
    //for(auto& entry: pairs) cout << entry.first.first << " , " << entry.first.second << endl;

    processor.mimForCollocations(singles, pairs, collocations);

    cout << "Getting Collocations From DB ............" << endl;
    for(auto& entry: collocations){
        //cout << "<" << entry.first << "," << entry.second << ">" << endl;
        if(entry.first != entry.second){
            coll.push_back(entry);
        }
    }


}


void Controller::handleTokenizeRequest(){
    qDebug() << "Tokenize button pressed";
}






////////////////////////////////////////////////////////////////
/////
///   UNIT TEST FUNCTIONS
///         Unit testing is supported for:
///             - Topic Extraction
///             - Classifying a mixed-character alphanumeric string
///             - Finding quasi-word collocations
///             - Classifying a quasi-word collocation
///             - Finding noun phrases in free text
///             - Finding verb phrase fragments in free text
///             - Classifying the supplier of an identified part number
///             - Entity deduplication
/////
///////////////////////////////////////////////////////////////


////////////////
/// \brief Controller::testBuildComponents
/// \return
///
int Controller::testBuildComponents(){
    return 0;
}

//TODO: broken
int Controller::testTopicExtraction(){
    /////obtain the text ..............................................
    ////tokenize the text .............................................
    //This collection is intended for use by fully implemented application
    vector<vector<string>*> testingSet = vector<vector<string>*>();

    //temporary text to test functions
    vector<string> text = vector<string>();

    cout << "Get test case" << endl;

    //if(getTextFromFile(text, tok) != 0) exit(-1);        // <--- USE THIS TO GET TEST CASE 1
    //if(getTestCase2(text, tok) != 0) exit(-1);             // <--- USE THIS TO GET TEST CASE 2
    //if(getTestCase3(text, tok) != 0) exit(-1);           // <--- USE THIS TO GET TEST CASE 3

    ////tag the text .................................................
    cout << "Tagging words" << endl;

    processor.getXML();
    processor.countTags();
    vector<pair<string,string>> tagResults = vector<pair<string,string>>();
    processor.tag(text, tagResults);

    ////topic analysis ..............................................

    cout << "TOPIC ANLYSIS..." << endl;
    cout << "Print topic words" << endl;
    //top.printTopicWords();
    enum enums::TOPIC topic = top.findTopic(text, repo);
    cout << "Topic: " << topic << endl;

    /*
    for(auto& entry: testingSet){
        currentTopic = top.findTopic(*entry);
        //do some stuff
    }*/

}


 // Finding quasi-word collocations

int Controller::testFindCollocations(){
    vector<pair<string,string>> colls = vector<pair<string,string>>();
    BayesianClassifier bayes = BayesianClassifier();
    vector<Component*> components = vector<Component*>();
    repo.getComponentsIncludingGenerics(components);
    getCollocationsFromDBDescriptions(colls);

    for(auto& entry: colls){
        map<string, float>* results = bayes.classifyCollocation(entry, components);

        cout << endl << endl << "CLASSIFY COLLOCATION: <" << entry.first << ", " << entry.second << ">" << endl;
        if(results != 0){

            for(auto& e2: (*results)){
                if(e2.second > 0) cout << e2.first << " : " << e2.second << endl;
            }
            delete results;

        } else cout << "Not found in corpus" << endl;

    }
}

//////
/// \brief Controller::testClassifyCollocations
/// \return success or failure
///
/// Test case for classifying some arbitrary quasi-word collocations
///
int Controller::testClassifyCollocations(){
    vector<pair<string, string>> testVector = vector<pair<string, string>>();
    BayesianClassifier bayes = BayesianClassifier();
    vector<Component*> components = vector<Component*>();
    repo.getComponentsIncludingGenerics(components);

    //the test cases
    testVector.push_back(make_pair("stainless", "steel"));
    testVector.push_back(make_pair("zinc", "blue"));
    testVector.push_back(make_pair("res", "0603"));
    testVector.push_back(make_pair("15", "pos"));
    testVector.push_back(make_pair("washer", "ss"));
    testVector.push_back(make_pair("fh", "zn"));
    testVector.push_back(make_pair("1206", "x5r"));
    testVector.push_back(make_pair("astm", "967"));
    testVector.push_back(make_pair("rubber", "bumper"));

    for(auto& entry: testVector){
        map<string, float>* results = bayes.classifyCollocation(entry, components);

        cout << endl << endl << "CLASSIFY COLLOCATION: <" << entry.first << ", " << entry.second << ">" << endl;
        if(results != 0){

            for(auto& e2: (*results)){
                if(e2.second > 0) cout << e2.first << " : " << e2.second << endl;
            }
            delete results;

        } else cout << "Not found in corpus" << endl;

    }


}

//todo: broken
int Controller::testNounPhrases(){

    //temporary text to test functions
    vector<string> text = vector<string>();

    cout << "Get test case" << endl;

    //if(getTextFromFile(text, tok) != 0) exit(-1);        // <--- USE THIS TO GET TEST CASE 1
    //if(getTestCase2(text, tok) != 0) exit(-1);             // <--- USE THIS TO GET TEST CASE 2
    //if(getTestCase3(text, tok) != 0) exit(-1);           // <--- USE THIS TO GET TEST CASE 3

    ////tag the text .................................................
    cout << "Tagging words" << endl;

    processor.getXML();
    processor.countTags();
    vector<pair<string,string>> tagResults = vector<pair<string,string>>();
    processor.tag(text, tagResults);

    /////////////////////////// SQL ERROR AFTER HERE  ----> /////////////////////////////////
    ////TODO: run the text through the technical dictionary ..........
    ///     This will idenify numbers, etc. that weare interested in.
    ///     Needs to run after the tagging from corpus -- will include special tags
    //processor.openTechDictionary(repo);
    //processor.applyTechDictionary(tagResults);
    //////classify the unidentified alphanumeric strings ..............
    //classifyAlpha("hi");
    ///////////////////////////// <---- SQL ERROR BEFORE HERE


    ////Parse the noun and verb phrases ......................
    vector<vector<pair<string, string>>*> nPhrases = vector<vector<pair<string, string>>*>();

    //this collection will hold the words we are analyzing
    vector<string> dwgText = vector<string>();

    //function that retrieves the test case
    //getTestCase2(dwgText, tok);

    ////////// Code that can be used to open training text for testing instead
    //repo.getAllDwgTextFromDB(dwgText);
    //repo.getAllDescriptionsFromDB(dwgText);

    ////// Extracts noun and verb phrases from the free text in dwgText

    processor.tag(dwgText, tagResults);
    processor.getNounPhrases(tagResults, nPhrases);

    for(auto& entry: nPhrases){
        cout << "NOUN PHRASE : " << endl;
        for(auto& e2: (*entry)){
            cout << e2.first << endl;
        }
        cout << endl;
    }

    return 0;

}

//todo: broken
int Controller::testVerbPhrases(){
    //temporary text to test functions
    vector<string> text = vector<string>();

    //if(getTextFromFile(text, tok) != 0) exit(-1);        // <--- USE THIS TO GET TEST CASE 1
    //if(getTestCase2(text, tok) != 0) exit(-1);             // <--- USE THIS TO GET TEST CASE 2
    //if(getTestCase3(text, tok) != 0) exit(-1);           // <--- USE THIS TO GET TEST CASE 3


    ////tag the text .................................................
    cout << "Tagging words" << endl;

    processor.getXML();
    processor.countTags();
    vector<pair<string,string>> tagResults = vector<pair<string,string>>();
    processor.tag(text, tagResults);

    /////////////////////////// SQL ERROR AFTER HERE  ----> /////////////////////////////////
    ////TODO: run the text through the technical dictionary ..........
    ///     This will idenify numbers, etc. that weare interested in.
    ///     Needs to run after the tagging from corpus -- will include special tags
    //processor.openTechDictionary(repo);
    //processor.applyTechDictionary(tagResults);
    //////classify the unidentified alphanumeric strings ..............
    //classifyAlpha("hi");
    ///////////////////////////// <---- SQL ERROR BEFORE HERE


    ////Parse the noun and verb phrases ......................
     vector<vector<pair<string, string>>*> vPhrases = vector<vector<pair<string, string>>*>();

    //this collection will hold the words we are analyzing
    vector<string> dwgText = vector<string>();

    //function htat retrieves the test case
    //getTestCase2(dwgText, tok);

    ////////// Code that can be used to open training text for testing instead
    //repo.getAllDwgTextFromDB(dwgText);
    //repo.getAllDescriptionsFromDB(dwgText);

    /////////// Legacy code that was used to obtain untagged words from the training set
    //obtainUntaggedWords(tagResults);

    ////// Extracts noun and verb phrases from the free text in dwgText

    processor.tag(dwgText, tagResults);
    processor.getVerbPhrases(tagResults, vPhrases);

    for(auto& entry: vPhrases){
        cout << "VERB PHRASE : " << endl;
        for(auto& e2: (*entry)){
            cout << e2.first << endl;
        }
        cout << endl;
    }

    return 0;

}


//////////////////////////////////
/// This code gets the compoents and uses Baysian learning to classify their Material-Article Type
///
int Controller::classifyMatArtType(string val){

    //Repository repo = Repository();
    /*vector<Component*> collection = vector<Component*>();
    repo.getComponents(collection);

    BayesianClassifier bayes = BayesianClassifier();
    bayes.learn(collection);

    string testComp = "CRCW060312K0JNEA";
    map<string,float>* results = bayes.classifySupplier(testComp, collection);

    if(results == 0) return -1;

    cout << endl << endl << "CLASSIFY " << testComp << endl;

    float tot = 0.0;

    for(auto& entry: (*results)){
        if(entry.second != 0) cout << entry.first << " : " << entry.second << endl;
        tot += entry.second;
    }

    cout << "Tot " << tot << endl;*/
    cout << "Wrong function" << endl;


    //delete results;
    return 0;


    /////// Cross Validation
    //crossValidate(bayes, collection);

    //map<string, float>* results = bayes.classify(testComp, collection);

    //int i = 0;

    /*for(auto& entry: *results){
        cout << entry.first << " .. " << entry.second << endl;
    }*/

    return 0;

}


/////////
/// \brief Controller::testClassifyingString
/// \return
/// Test classifier for classifying a Component's material or article type given its part number
int Controller::testClassifyMatType(){
    vector<Component*> comps = vector<Component*>();
    repo.getComponents(comps);

    BayesianClassifier bayes = BayesianClassifier();
    bayes.learn(comps);

    string testComp = "CRCW060312K0JNEA";
    map<string,float>* results = bayes.classifyType(testComp, comps);

    if(results == 0) return -1;

    cout << endl << endl << "CLASSIFY " << testComp << endl;

    for(auto& entry: (*results)){
        if(entry.second != 0) cout << entry.first << " : " << entry.second << endl;
    }

    pair<string, float> choice = UtilityAlgorithms::argmax(results);

    cout << "Choice : " << choice.first << endl;

    delete results;

    crossValidateType(bayes, comps);

    return 0;

}


/////////
/// \brief Controller::testClassifySupplier
/// \return
///  Test classifier for supplier given MPN
int Controller::testClassifySupplier(){
    vector<Component*> comps = vector<Component*>();
    repo.getComponents(comps);

    BayesianClassifier bayes = BayesianClassifier();
    bayes.learn(comps);

    string testComp = "CRCW060312K0JNEA";
    map<string,float>* results = bayes.classifySupplier(testComp, comps);

    if(results == 0) return -1;

    cout << endl << endl << "CLASSIFY " << testComp << endl;

    for(auto& entry: (*results)){
        if(entry.second != 0) cout << entry.first << " : " << entry.second << endl;
    }

    pair<string, float> choice = UtilityAlgorithms::argmax(results);

    cout << "Choice : " << choice.first << endl;

    delete results;

    crossValidateSupp(bayes, comps);

    return 0;
}

int Controller::testEntityDeduplication(){return 0;} //Entity deduplication

void Controller::testParent(){
    map<string, float> res = map<string, float>();
    cout << "Test parents" << endl;
    vector<Component*> comps = vector<Component*>();

    repo.getFunctionalTestComponents(comps);

    vector<Component*>::iterator it;
    for(it = comps.begin(); it <= comps.begin() + 10; ++it){
        auto res = map<string, float>();
        repo.getParentTypes(res, (*it)->type);
        cout << endl << *(*it) << endl;
        for(auto& entry: res){
            cout << entry.first << " : " << entry.second << endl;
        }

    }


    //string t = "Subassembly";
    //repo.getParentTypes(res, t);
}






/////////////
/// getTextFromFile()
/// gets the text from a file
/// Used to otain the text from testcase.txt
int Controller::getTextFromFile(vector<string>& text, Tokenizer& tok){

    //open the file -- static location for now

    ifstream thefile("/home/ian/Data/testcase.txt");
    string line = "";
    if (thefile.is_open()) {
      while ( getline (thefile,line) ) {
          QString theLine = QString::fromStdString(line);
          QStringList pieces = theLine.split(' ');
          for(auto& entry: pieces){
              tok.removeStopCharacters(entry);
              string s = entry.toStdString();
              text.push_back(s);//QString::toStdString(entry));
          }
      }

      thefile.close();
      return 0;
    }
    return -1;
}


///////////////
/// TOKENIZE
/// Tokenizes a text file given by fileName
///
///
int Controller::tokenize(string fileName){

    Tokenizer* tok = new Tokenizer(fileName);
    tok->tokenize();
    //tok->writeToSQL();

    delete tok;

    return 0;
}

///////////////
/// \brief Controller::crossValidateType
/// \param bayes
/// \param collection
/// Used when performing cross-validation to test the Bayesian component classifier
///
void Controller::crossValidateType(BayesianClassifier& bayes, vector<Component*>& collection){
    //Component* testComp = new Component("Bob's bolts", "CRCW040222R0FKED", "Some widget", "");
    vector<Component*>::iterator it;
    int right = 0;
    int wrong = 0;
    int i = 0;

    for(int fourfold = 0; fourfold < 4; ++fourfold ){

        list<Component*> training = list<Component*>();
        vector<Component*> testing = vector<Component*>();

        for(it = collection.begin(); it != collection.end(); ++i ,++it){
            if(i % 4 == fourfold){
                testing.push_back(*it);
            } else training.push_back(*it);
        }

        vector<Component*> training2;
        training2.reserve(training.size());
        copy(begin(training), end(training), back_inserter(training2));

        cout << "Size of training: " << collection.size() << "  Testing: " << testing.size() << endl;

        bayes.learn(training2);

        cout << "Learning" << endl;


        for(Component* c: testing){
            map<string, float>* results = bayes.classifyType(c, collection);
            auto choice = std::max_element(results->begin(), results->end(),
                [](const pair<string, float>& p1, const pair<string, float>& p2) {
                    return p1.second < p2.second; });
            if((*choice).first.compare(c->type) == 0) right++; else wrong++;
                cout << "Right: " << right << " Wrong: " << wrong << endl;
            delete results;
        }
    }

    float res = (float) right / (float) (right + wrong);

    cout << "Accuracy: %" << (res * 100) << endl;

    //delete testComp;
}

///////////////
/// \brief Controller::crossValidateSupp
/// \param bayes
/// \param collection
/// Used when performing cross-validation to test the Bayesian component classifier
///
void Controller::crossValidateSupp(BayesianClassifier& bayes, vector<Component*>& collection){
    //Component* testComp = new Component("Bob's bolts", "CRCW040222R0FKED", "Some widget", "");

    vector<Component*>::iterator it;
    int right = 0;
    int wrong = 0;
    int i = 0;

    for(int fourfold = 0; fourfold < 4; ++fourfold ){
        vector<Component*> testing = vector<Component*>();
        list<Component*> training = list<Component*>();

        for(it = collection.begin(); it != collection.end(); ++i ,++it){
            if(i % 4 == fourfold){
                testing.push_back(*it);
            } else training.push_back(*it);
        }

        vector<Component*> training2;
        training2.reserve(training.size());
        copy(begin(training), end(training), back_inserter(training2));

        cout << "Size of training: " << collection.size() << "  Testing: " << testing.size() << endl;

        bayes.learn(training2);

        cout << "Learning" << endl;


        for(Component* c: testing){
            map<string, float>* results = bayes.classifySupplier(c, collection);
            auto choice = std::max_element(results->begin(), results->end(),
                [](const pair<string, float>& p1, const pair<string, float>& p2) {
                    return p1.second < p2.second; });
            if((*choice).first == /*c->supplierNumber*/ c->mfr) right++; else wrong++;
                cout << "Right: " << right << " Wrong: " << wrong << endl;
            delete results;
        }

    }

    float res = (float) right / (float) (right + wrong);

    cout << "Accuracy: %" << (res * 100) << endl;

    //delete testComp;
}




/////////
/// \brief Controller::runOneOf
/// Set this handler to whatever one-of function needs to be run for test or data cleaning
void Controller::runOneOf(){
    vector<pair<string,string>> terms = vector<pair<string,string>>();

    repo.getTechKeywords(terms);

    for(auto& entry: terms){
        cout << entry.first << " : " << entry.second << endl;
    }

    //repo.replaceParents();
}

/*
    //determine the topic
    for(auto& entry: files){
        entry->topic = top.findTopic(entry->words, repo);
    }

    //scan for parents
    //if another file's f1 name is found in the text of a file f2, then f1's parent is f2
    for(auto& e1: files){
        for(auto& e2: e1->words){
            for(auto& e3: files){
                //TODO: strip off "txt" and ".pdf" from filenames
                if(e3 != e1 && e3->filename == e2){
                    e3->parent = e1;
                }
            }
        }
    }

    //tag the words
    processor.getXML();
    processor.countTags();
    for(auto& entry: files){
        processor.tag(entry->words, entry->tags);
    }


    //look for collocations
    const int SEARCHDIST = 4;
    vector<pair<string,string>> colls = vector<pair<string, string>>();
    //processor.findCollocationMetrics();
    getCollocationsFromDBDescriptions(colls);
    vector<string>::iterator it3, it4;
    for(auto& entry: files){
        for(it3 = entry->words.begin(); it3 != entry->words.end(); ++it3){
            if(*it3 != "~"){
                for(it4 = it3+1; it4 != entry->words.end() && it4 <= it3 + SEARCHDIST && *it4 != "~"; ++it4){
                    for(auto& e3: colls){
                        if(e3.first == *it3 && e3.second == *it4){
                            (*entry).collocations[e3] = "0";
                        }
                    }
                }
            }
        }
    }

    //classify collocations
    vector<Component*> components = vector<Component*>();
    repo.getComponentsIncludingGenerics(components);
    getCollocationsFromDBDescriptions(colls);
    vector<Component*> compsIncGenerics = vector<Component*>();
    repo.getComponentsIncludingGenerics(compsIncGenerics);

    vector<string> supplierNames = vector<string>();
    repo.getSupplierNames(supplierNames);

    for(auto& entry: files){
        for(auto& e2: entry->collocations){
            pair<string,string> coll = make_pair(e2.first.first, e2.first.second);
            map<string, float>* results = bayes.classifyCollocation(coll, compsIncGenerics);
            cout << endl << endl << "CLASSIFY COLLOCATION: <" << coll.first << ", " << coll.second << ">" << endl;
            if(results != 0){

                for(auto& e3: (*results)){
                    if(e3.second > 0) cout << e3.first << " : " << e3.second << endl;
                }
                delete results;

            } else cout << "Not found in corpus" << endl;

        }
    }


    //Tag manufacturer names
    for(auto& e1:files){
        for(auto& e2: e1->tags){
            if(e2.first == "~") e2.second = "DOT";
            else
            for(auto& e3: supplierNames){
                if(e3 == e2.first){
                    e2.second = "NNP type=MFR";
                }
            }
        }
    }

    //build noun phrases
    //build verb phrases
    for(auto& entry: files){
        processor.getNounPhrases(entry->tags, entry->nounPhrases);
        processor.getVerbPhrases(entry->tags, entry->verbPhrases);
    }


    //lookup mixed alphanumerics

    vector<Component*> collection = vector<Component*>();
    repo.getComponents(collection);
    bayes.learn(collection);


    //classify untagged words
    //TODO: remove words in collocations
    for(auto& e1: files){
        for(auto& e2: e1->tags){
            if(e2.second == "???"){
                map<string, float>* results = new map<string,float>();
                results = bayes.classifyType(e2.first, collection);
                if(results->size() > 0){
                    auto choice = std::max_element(results->begin(), results->end(),
                        [](const pair<string, float>& p1, const pair<string, float>& p2) {
                            return p1.second < p2.second; });
                    if(choice->second > 0.1)
                        e2.second = "NN type=" + choice->first;
                    else e2.second = "UNCLASSIFIED";
                }
                delete results;
            }
        }
    }

    //classify remaining mixed alphanumerics
    //match MPN and MFR and MFR - MPN matches
    //Deduplicate generics
    //build component hierarchy

    ////// Display results for testing purposes
    for(auto& entry: files){
        cout << endl << "FILE: " << entry->filename << "       TOPIC: " << enums::topicStrings[entry->topic] << " ....... " << endl;
        cout << "PARENT: " << (entry->parent == 0 ? "ROOT" : entry->parent->filename) << endl;
        for(auto& e2: entry->tags){
            cout << e2.first << "   " << e2.second << endl;
        }
        cout << "Collocations..." << endl;
        for(auto& e2: entry->collocations){
            cout << "<" << e2.first.first << ", " << e2.first.second << ">" << endl;
        }
        cout << "Noun Phrases..." << endl;
        for(auto& e2: entry->nounPhrases){
            for(auto& e3: (*e2)){
                cout << e3.first << "..";
            }
            cout << endl;
        }

        cout << "Verb Phrases..." << endl;
        for(auto& e2: entry->verbPhrases){
            for(auto& e3: (*e2)){
                cout << e3.first << "..";
            }
            cout << endl;
        }

    }
*/
