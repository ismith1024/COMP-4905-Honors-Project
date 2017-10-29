#include "topicanalyzer.h"

TopicAnalyzer::TopicAnalyzer(Repository& repo){
    //repo = rep;
    //tok = tk;
    aggregateCounts = map<string, int>();
    metalCounts = map<string, int>();
    plasticCounts = map<string, int>();
    cableCounts = map<string, int>();
    assemblyCounts = map<string, int>();
    otherCounts = map<string, int>();
    pcbaCounts = map<string, int>();
    labelCounts = map<string, int>();
    electronicsCounts = map<string, int>();
    packagingCounts = map<string, int>();

    /*ifstream xmlfile("/home/ian/Data/Corpus.xml");
    string line = "";

    //this gets the Brown Corpus
    if (xmlfile.is_open() && xml2.is_open()) {
        while ( getline (xmlfile,line) ) {
          QString qs = QString::fromStdString(line);
          QStringList pieces = qs.split("<");

          for(QString st: pieces){
              if(st.size() > 0 && st.at(0) == 'w'){
                  QStringList pieces2 = st.split('>');

                  if(pieces2.size() > 1 ){
                      QString s = pieces2.at(0);
                      string s1 = s.replace("w type=\"", "").replace("\"","").toStdString();
                      s1 = tolower(s1);
                      aggregateCounts[s1]++;
                  }
              }
          }
        }
    }
    xmlfile.close();*/

    repo.getTopicCounts(metalCounts, enums::METAL);
    repo.getTopicCounts(plasticCounts, enums::PLASTIC);
    repo.getTopicCounts(cableCounts, enums::CABLE);
    repo.getTopicCounts(assemblyCounts, enums::ASSEMBLY);
    repo.getTopicCounts(otherCounts, enums::OTHER);
    repo.getTopicCounts(pcbaCounts, enums::PCBA);
    repo.getTopicCounts(labelCounts, enums::LABEL);
    repo.getTopicCounts(electronicsCounts, enums::ELECTRONICS);
    repo.getTopicCounts(packagingCounts,  enums::PACKAGING);

    ////NOT WORKING

    for(auto& entry: metalCounts){aggregateCounts[entry.first]+= metalCounts[entry.first];}
    for(auto& entry: plasticCounts){aggregateCounts[entry.first]+= plasticCounts[entry.first];}
    for(auto& entry: cableCounts){aggregateCounts[entry.first]+=cableCounts[entry.first];}
    for(auto& entry: assemblyCounts){aggregateCounts[entry.first]+=assemblyCounts[entry.first];}
    for(auto& entry: otherCounts){aggregateCounts[entry.first]+=otherCounts[entry.first];}
    for(auto& entry: pcbaCounts){aggregateCounts[entry.first]+=pcbaCounts[entry.first];}
    for(auto& entry: labelCounts){aggregateCounts[entry.first]+=labelCounts[entry.first];}
    for(auto& entry: electronicsCounts){aggregateCounts[entry.first]+=electronicsCounts[entry.first];}
    for(auto& entry: packagingCounts){aggregateCounts[entry.first]+=packagingCounts[entry.first];}


}


void TopicAnalyzer::printTopicWords(){

    //Will consider the word interesting if it appears k% more frequently than usual
    /////////////TODO: set these numbers to whatever works best
    const float THRESHOLD = 0.3 * (1.0/9.0);
    const int SUPPORT = 5;
    cout << endl <<"..METAL................" << endl;

    for(auto& entry: metalCounts){
        float ratio = ((float) metalCounts[entry.first]) / ((float) aggregateCounts[entry.first]);
        if(ratio > THRESHOLD && metalCounts[entry.first] >= SUPPORT) cout << entry.first << " .... ratio: " << ratio << " ......support: " << metalCounts[entry.first] << endl;
    }

    cout << endl << "..PLASTIC................" << endl;

    for(auto& entry: plasticCounts){
        float ratio = ((float) plasticCounts[entry.first]) / ((float) aggregateCounts[entry.first]);
        if(ratio > THRESHOLD && plasticCounts[entry.first] >= SUPPORT) cout << entry.first << " .... ratio: " << ratio << " ......support: " << plasticCounts[entry.first] <<  endl;
    }

    cout << endl << "..CABLE................" << endl;

    for(auto& entry: cableCounts){
        float ratio = ((float) cableCounts[entry.first]) / ((float) aggregateCounts[entry.first]);
        if(ratio > THRESHOLD && cableCounts[entry.first] >= SUPPORT) cout << entry.first << " .... ratio: " << ratio << " ......support: " << cableCounts[entry.first] <<  endl;
    }
    cout << endl << "..ASSEMBLY................" << endl;

    for(auto& entry: assemblyCounts){
        float ratio = ((float) assemblyCounts[entry.first]) / ((float) aggregateCounts[entry.first]);
        if(ratio > THRESHOLD && assemblyCounts[entry.first] >= SUPPORT) cout << entry.first << " .... ratio: " << ratio << " ......support: " << assemblyCounts[entry.first] <<  endl;
    }
    cout << endl << "..OTHER................" << endl;

    for(auto& entry: otherCounts){
        float ratio = ((float) otherCounts[entry.first]) / ((float) aggregateCounts[entry.first]);
        if(ratio > THRESHOLD && otherCounts[entry.first] >= SUPPORT) cout << entry.first << " .... ratio: " << ratio << " ......support: " << otherCounts[entry.first] <<  endl;
    }
    cout << endl << "..PCBA................" << endl;

    for(auto& entry: pcbaCounts){
        float ratio = ((float) pcbaCounts[entry.first]) / ((float) aggregateCounts[entry.first]);
        if(ratio > THRESHOLD && pcbaCounts[entry.first] >= SUPPORT) cout << entry.first << " .... ratio: " << ratio << " ......support: " << pcbaCounts[entry.first] <<  endl;
    }
    cout << "..LABEL................" << endl;

    for(auto& entry: labelCounts){
        float ratio = ((float) labelCounts[entry.first]) / ((float) aggregateCounts[entry.first]);
        if(ratio > THRESHOLD && labelCounts[entry.first] >= SUPPORT) cout << entry.first << " .... ratio: " << ratio << " ......support: " << labelCounts[entry.first] <<  endl;
    }
    cout << "..ELECTRONICS................" << endl;

    for(auto& entry: electronicsCounts){
        float ratio = ((float) electronicsCounts[entry.first]) / ((float) aggregateCounts[entry.first]);
        if(ratio > THRESHOLD && electronicsCounts[entry.first] >= SUPPORT) cout << entry.first << " .... ratio: " << ratio << " ......support: " << electronicsCounts[entry.first] <<  endl;
    }
    cout << "..PACKEGING................" << endl;

    for(auto& entry: packagingCounts){
        float ratio = ((float) packagingCounts[entry.first]) / ((float) aggregateCounts[entry.first]);
        if(ratio > THRESHOLD && packagingCounts[entry.first] >= SUPPORT) cout << entry.first << " .... ratio: " << ratio << " ......support: " << packagingCounts[entry.first] <<  endl;
    }



}



//////
/// \brief TopicAnalyzer::findTopic
/// \return
///Finds the most likely topic for a string representing the text from a file
/// This is accomplished by hashing the dictionary
enum enums::TOPIC TopicAnalyzer::findTopic(string str, Repository& repo){

    return enums::OTHER;
}


////////
/// \brief TopicAnalyzer::findTopic
/// \return
/// Finds the most likely topic for a collection of strings representing the text from a file
enum enums::TOPIC TopicAnalyzer::findTopic(vector<string>& coll, Repository& repo){

    string str = "";
    for(auto& entry: coll){str += entry + " ";}

    return findTopic(str, repo);
}
