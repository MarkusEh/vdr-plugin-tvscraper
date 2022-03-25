#include <string>
#include <sstream>
#include <vector>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "tvdbactors.h"

using namespace std;

cTVDBActors::cTVDBActors(string language) {
    this->language = language;
}

cTVDBActors::~cTVDBActors() {
    actors.clear();
}

void cTVDBActors::ReadActors(xmlDoc *doc, xmlNode *nodeActors) {
    for (xmlNode *cur_node = nodeActors->children; cur_node; cur_node = cur_node->next) {
        if ((cur_node->type == XML_ELEMENT_NODE) && !xmlStrcmp(cur_node->name, (const xmlChar *)"Actor"))
            ReadEntry(doc, cur_node->children);
   }
}
void cTVDBActors::ReadEntry(xmlDoc *doc, xmlNode *node) {
    xmlNode *cur_node = NULL;
    xmlChar *node_content;
    cTVDBActor *actor = new cTVDBActor();
    for (cur_node = node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            node_content = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
            if (!node_content)
                continue;
            if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Image")) {
                actor->path = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Name")) {
                actor->name = (const char *)node_content;
            } else if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Role")) {
                actor->role = (const char *)node_content;
            }
            xmlFree(node_content);
        }
    }
    actors.push_back(actor);
}

void cTVDBActors::StoreDB(cTVScraperDB *db, int series_id) {
    for (size_t i=0; i<actors.size(); i++) {
      if (actors[i]->path.empty() ) {
        db->InsertActor(series_id, actors[i]->name, actors[i]->role, -1);
      } else {
        db->InsertActor(series_id, actors[i]->name, actors[i]->role, i);
        db->AddActorDownload(series_id * -1, false, i, actors[i]->path);
      }
    }
}

void cTVDBActors::Dump(bool verbose) {
    int size = actors.size();
    esyslog("tvscraper: read %d actor entries", size);
    if (!verbose)
        return;
    for (int i=0; i<size; i++) {
        esyslog("tvscraper: path %s, name: %s, role %s", actors[i]->path.c_str(), actors[i]->name.c_str(), actors[i]->role.c_str());
    }
}
