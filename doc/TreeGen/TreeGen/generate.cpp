#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <queue>
#include <XmlReader.h>

struct Path{
    std::string child;
    std::vector<std::string> parents;

    Path(const std::string &startNode):
        child(startNode)
    {
        parents.push_back(startNode);
    }
};

enum EdgeType{
    GATHER,
    CONSTRUCT_FROM_ITEM,
    GATHER_REQ,
    LOOT,

    UNLOCK_ON_GATHER,
    UNLOCK_ON_ACQUIRE,
    UNLOCK_ON_CRAFT,
    UNLOCK_ON_CONSTRUCT,

    DEFAULT
};

struct Edge{
    std::string parent, child;
    EdgeType type;
    
    Edge(const std::string &from, const std::string &to, EdgeType typeArg): parent(from), child(to), type(typeArg) {}
    bool operator==(const Edge &rhs) const { return parent == rhs.parent && child == rhs.child; }
    bool operator<(const Edge &rhs) const{
        if (parent != rhs.parent) return parent < rhs.parent;
        return child < rhs.child;
    }
};

int main(){
    std::set<Edge> edges;
    std::map<std::string, std::string> tools;
    std::map<std::string, std::string> nodes; // id -> label
    std::set<Edge > blacklist;

    std::string colorScheme = "rdylgn10";
    std::map<EdgeType, size_t> edgeColors;
    // Weak: light colors
    edgeColors[GATHER] = 1;
    edgeColors[UNLOCK_ON_ACQUIRE] = 2; // Weak, since you can trade for the item.
    edgeColors[CONSTRUCT_FROM_ITEM] = 3;
    edgeColors[LOOT] = 4;
    edgeColors[GATHER_REQ] = 5;
    edgeColors[UNLOCK_ON_GATHER] = 6;
    // Strong: dark colors
    edgeColors[UNLOCK_ON_CONSTRUCT] = 9;
    edgeColors[UNLOCK_ON_CRAFT] = 10;

    const std::string dataPath = "../../Data";

    // Load tools
    XmlReader xr("archetypalTools.xml");
    for (auto elem : xr.getChildren("tool")){
        std::string id, archetype;
        if (!xr.findAttr(elem, "id", id)) continue;
        if (!xr.findAttr(elem, "archetype", archetype)) continue;
        tools[id] = archetype;
    }

    // Load blacklist
    for (auto elem : xr.getChildren("blacklist")){
        std::string parent, child;
        if (!xr.findAttr(elem, "parent", parent)){
            std::cout << "Blacklist item had no parent; ignored" << std::endl;
            continue;
        }
        if (!xr.findAttr(elem, "child", child)){
            std::cout << "Blacklist item had no child; ignored" << std::endl;
            continue;
        }
        blacklist.insert(Edge(parent, child, DEFAULT));
    }

    // Load items
    if (!xr.newFile(dataPath + "/items.xml"))
        std::cerr << "Failed to load items.xml" << std::endl;
    else{
        for (auto elem : xr.getChildren("item")) {
            std::string id;
            if (!xr.findAttr(elem, "id", id))
                continue;
            std::string label = "item_" + id;

            std::string s;
            if (xr.findAttr(elem, "name", s))
                nodes.insert(std::make_pair(label, s));

            if (xr.findAttr(elem, "constructs", s)){
                    edges.insert(Edge(label, "object_" + s, CONSTRUCT_FROM_ITEM));
            }
        }
    }

    // Load objects
    if (!xr.newFile(dataPath + "/objectTypes.xml"))
        std::cerr << "Failed to load objectTypes.xml" << std::endl;
    else{
        for (auto elem : xr.getChildren("objectType")) {
            std::string id;
            if (!xr.findAttr(elem, "id", id))
                continue;
            std::string label = "object_" + id;

            std::string s;
            if (xr.findAttr(elem, "name", s))
                nodes.insert(std::make_pair(label, s));

            if (xr.findAttr(elem, "gatherReq", s)){
                auto it = tools.find(s);
                if (it == tools.end()){
                    std::cerr << "Tool class is missing archetype: " << s << std::endl;
                    edges.insert(Edge(s, label, GATHER_REQ));
                } else
                    edges.insert(Edge(it->second, label, GATHER_REQ));
            }

            for (auto yield : xr.getChildren("yield", elem)) {
                if (!xr.findAttr(yield, "id", s))
                    continue;
                edges.insert(Edge(label, "item_" + s, GATHER));
            }

            for (auto unlockBy : xr.getChildren("unlockedBy", elem)){
                if (xr.findAttr(unlockBy, "recipe", s) || xr.findAttr(unlockBy, "item", s))
                    edges.insert(Edge("item_" + s, label, UNLOCK_ON_CRAFT));
                else if (xr.findAttr(unlockBy, "construction", s))
                    edges.insert(Edge("object_" + s, label, UNLOCK_ON_CONSTRUCT));
                else if (xr.findAttr(unlockBy, "gather", s))
                    edges.insert(Edge("item_" + s, label, UNLOCK_ON_GATHER));
                else if (xr.findAttr(unlockBy, "item", s))
                    edges.insert(Edge("item_" + s, label, UNLOCK_ON_ACQUIRE));
            }
        }
    }

    // Load NPCs
    if (!xr.newFile(dataPath + "/npcTypes.xml"))
        std::cerr << "Failed to load npcTypes.xml" << std::endl;
    else{
        for (auto elem : xr.getChildren("npcType")) {
            std::string id;
            if (!xr.findAttr(elem, "id", id))
                continue;
            std::string label = "npc_" + id;

            std::string s;
            if (xr.findAttr(elem, "name", s))
                nodes.insert(std::make_pair(label, s));

            for (auto loot : xr.getChildren("loot", elem)) {
                if (!xr.findAttr(loot, "id", s))
                    continue;
                edges.insert(Edge(label, "item_" + s, LOOT));
            }
        }
    }


    // Load recipes
    if (!xr.newFile(dataPath + "/recipes.xml"))
        std::cerr << "Failed to load recipes.xml" << std::endl;
    else{
        for (auto elem : xr.getChildren("recipe")) {
            std::string product;
            if (!xr.findAttr(elem, "product", product))
                continue;
            std::string label = "item_" + product;

            std::string s;

            for (auto unlockBy : xr.getChildren("unlockedBy", elem)){
                if (xr.findAttr(unlockBy, "recipe", s) || xr.findAttr(unlockBy, "item", s))
                    edges.insert(Edge("item_" + s, label, UNLOCK_ON_CRAFT));
                else if (xr.findAttr(unlockBy, "construction", s))
                    edges.insert(Edge("object_" + s, label, UNLOCK_ON_CONSTRUCT));
                else if (xr.findAttr(unlockBy, "gather", s))
                    edges.insert(Edge("item_" + s, label, UNLOCK_ON_GATHER));
                else if (xr.findAttr(unlockBy, "item", s))
                    edges.insert(Edge("item_" + s, label, UNLOCK_ON_ACQUIRE));
            }
        }
    }

    // Remove blacklisted items
    for (auto blacklistedEdge : blacklist)
        for (auto it = edges.begin(); it != edges.end(); ++it)
            if (*it == blacklistedEdge){
                edges.erase(it);
                break;
            }


    // Remove shortcuts
    for (auto edgeIt = edges.begin(); edgeIt != edges.end(); ){
        /*
        This is one edge.  We want to figure out if there's a longer path here
        (i.e., this path doesn't add any new information about progress requirements).
        */
        bool shouldDelete = false;
        std::queue<std::string> queue;
        std::set<std::string> nodesFound;

        // Populate queue initially with all direct children
        for (const Edge &edge : edges)
            if (edge.parent == edgeIt->parent && edge.child != edgeIt->child)
                queue.push(edge.child);

        // New nodes to check will be added here.  Effectively it will be a breadth-first search.
        while (!queue.empty() && !shouldDelete){
            std::string nextParent = queue.front();
            queue.pop();
            for (const Edge &edge : edges){
                if (edge.parent != nextParent)
                continue;
                if (edge.child == edgeIt->child){
                    // Mark the edge for removal
                    shouldDelete = true;
                    break;
                } else if (nodesFound.find(edge.child) != nodesFound.end()){
                    queue.push(edge.child);
                    nodesFound.insert(edge.child);
                }
            }
        }
        auto nextIt = edgeIt; ++nextIt;
        if (shouldDelete)
            edges.erase(edgeIt);
        edgeIt = nextIt;
    }


    // Remove loops
    // First, find set of nodes that start edges but don't finish them (i.e., the roots of the tree).
    std::set<std::string> starts, ends;
    for (auto &edge : edges){
        starts.insert(edge.parent);
        ends.insert(edge.child);
    }
    for (const std::string &endNode : ends){
        starts.erase(endNode);
    }

    // Next, do a BFS from each to find loops.  Remove those final edges.
    std::set<Edge > trashCan;
    for (const std::string &startNode : starts){
        //std::cout << "Root node: " << startNode << std::endl;
        std::queue<Path> queue;
        queue.push(Path(startNode));
        bool loopFound = false;
        while (!queue.empty() && !loopFound){
            Path nextPath = queue.front();
            queue.pop();
            for (auto it = edges.begin(); it != edges.end(); ++it){
                if (it->parent != nextPath.child)
                    continue;
                std::string child = it->child;
                // If this child is already a parent
                if (std::find(nextPath.parents.begin(), nextPath.parents.end(), child) != nextPath.parents.end()){
                    std::cout << "Loop found; marked for removal:" << std::endl << "  ";
                    for (const std::string &parent : nextPath.parents)
                        std::cout << parent << " -> ";
                    std::cout << child << std::endl;
                    // Mark the edge for removal
                    trashCan.insert(*it);
                } else {
                    Path p = nextPath;
                    p.parents.push_back(child);
                    p.child = child;
                    queue.push(p);
                }
            }
        }
        for (auto edgeToDelete : trashCan)
            for (auto it = edges.begin(); it != edges.end(); ++it)
                if (*it == edgeToDelete){
                    edges.erase(edgeToDelete);
                    break;
                }
    }



    // Publish
    std::ofstream f("tree.gv");
    f << "digraph {" << std::endl;
    f << "node [fontsize=10 shape=box];" << std::endl;

    // Nodes
    for (auto &node : nodes){
        std::string imagePath = "../../Images/";
        if (node.first.substr(0, 5) == "item_")
            imagePath += "items/" + node.first.substr(5);
        else if (node.first.substr(0, 4) == "npc_")
            imagePath += "NPCs/" + node.first.substr(4);
        else
            imagePath += "objects/" + node.first.substr(7);

        imagePath += ".png";
        std::string
            id = node.first,
            name = node.second,
            image = "", //"<img src=\"" + imagePath + "\"/>",
            fullNode = node.first + " [label=<<table border='0' cellborder='0'><tr><td>" + image + "</td></tr><tr><td>" + name + "</td></tr></table>>]";
        f << fullNode << std::endl;
    }

    for (auto &edge : edges)
        f << edge.parent << " -> " << edge.child
          << " [colorscheme=\"" << colorScheme << "\", color=" << edgeColors[edge.type] << "]"
          << std::endl;
    f << "}";
}
