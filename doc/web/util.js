function getID(){
    var url = location.search;
    var params = new Object();
    if (url.indexOf("?") != -1) {
        var str = url.substr(1);
        strs = str.split("&");
        for(var i = 0; i < strs.length; i ++) {
            var splitStr = strs[i].split("=");
            var paramName = splitStr[0];
            var val = splitStr[1];
            if (params.hasOwnProperty(paramName)){
                // Multiple references to the same param; construct array
                if (isArray(params[paramName])){
                    params[paramName].push(val);
                } else {
                    params[paramName] = [params[paramName], val];
                }
            } else {
                params[paramName] = unescape(val);
            }
        }
    }
    if ("id" in params)
        return params.id;
    return "";
}

function findObject(id = getID()){
    return findByID(objects, id);
}

function findItem(id = getID()){
    return findByID(items, id);
}

function findNPC(id = getID()){
    return findByID(npcs, id);
}

function findTag(id = getID()){
    return findByID(tags, id);
}


function findByCustom(collection, key, keyName){
    var i;
    for (i = 0; i < collection.length; ++i){
        var entry = collection[i];
        if (entry[keyName] == key)
            return entry;
    }
    return {};
}
function findByID(collection, id){
    return findByCustom(collection, id, "id");
}

function imageNode(entry){
    return '<img src="images/' + entry.image + '.png"/>';
}

function displayTimeAsHMS(ms){
    var displayString = "";
    var seconds = parseInt(ms) / 1000.0;
    if (seconds >= 3600){
        var hours = parseInt(seconds / 3600);
        seconds = seconds % 3600;
        displayString += hours + "h";
    }
    if (seconds >= 60){
        var minutes = parseInt(seconds / 60);
        seconds = seconds % 60;
        displayString += minutes + "m";
    }
    if (seconds > 0)
        displayString += seconds + "s"
    return displayString;
}

function textOnlyObjectLink(object){
    var linkHTML =
        '<a href="object.html?id=' + object.id + '">'
            + "&#9820; " + object.name
        + '</a>';
    return linkHTML;
}

function objectLink(object){
    var linkHTML =
        '<a href="object.html?id=' + object.id + '">'
            + object.name
            + '<br>' + imageNode(object)
        + '</a>';
    return linkHTML;
}

function textOnlyNPCLink(npc){
    var linkHTML =
        '<a href="npc.html?id=' + npc.id + '">'
            + "	&#9822; " + npc.name
        + '</a>';
    return linkHTML;
}

function npcLink(npc){
    var linkHTML =
        '<a href="npc.html?id=' + npc.id + '">'
            + npc.name
            + '<br>' + imageNode(npc)
        + '</a>';
    return linkHTML;
}

function textOnlyItemLink(item){
    var linkHTML =
        '<a href="item.html?id=' + item.id + '">'
            + (isGear(item) ? "&#9876; " : "&#9752; ")
            + item.name
        + '</a>';
    return linkHTML;
}

function itemLink(item){
    var linkHTML = 
        '<a href="item.html?id=' + item.id + '">'
            + imageNode(item)
            + ' ' + item.name
        + '</a>';
    return linkHTML;
}

function textOnlyRecipeLink(recipe){
    var linkHTML =
        "	&#9879; " + recipe.id
    return linkHTML;
}

function textOnlySpellLink(spell){
    var linkHTML =
        "	&#9889; " + spell.name
    return linkHTML;
}

function tagLink(tag){
    var linkHTML = 
        '<a href="tag.html?id=' + tag.id + '">'
            + tag.name
        + '</a>';
    return linkHTML;
}

function compileUnlockListItem(lock){
    console.log(lock);
    var link;
    if (linkPageByType[lock.type] == "item")
        link = itemLink(findItem(lock.sourceID));
    else
        link = objectLink(findObject(lock.sourceID));
    
    var linkAddress = linkPageByType[lock.type] + ".html?id=" + lock.sourceID;
    var listText =
            '<li>' + prefixByType[lock.type]
            + '<a href="' + linkAddress + '">' + link + '</a>'
            + suffixByType[lock.type] +  '</li>';
    return listText;
}

function scalarToPercent(scalar){
    var percentage = Math.round((parseFloat(scalar) - 1) * 100);
    var prefix = percentage < 0 ? '-' : '+';
    return prefix + percentage + '%';
}
    
function sortByName(a, b){
    var aName = a.hasOwnProperty("name") ? a.name : a.id;
    var bName = b.hasOwnProperty("name") ? b.name : b.id;
    var nameCompare = aName.localeCompare(bName);
    if (nameCompare != 0)
        return nameCompare;
    return a.id.localeCompare(b.id);
}

function isGear(item){
    return "gearSlot" in item;
}

function loadNavBar(){
    $("#navBarGetsLoadedInHere").load("navBar.html");
}

function buildListFromArray(array){
    var list = "";
    for (var i = 0; i < array.length; ++i){
        var entry = array[i];
        if (i > 0)
            list += ", ";
        list += entry;
    }
    return list;
}

function ms2s(ms){ return Math.round(0.01 * ms) / 10; }

objects.sort(sortByName);
npcs.sort(sortByName);
items.sort(sortByName);
