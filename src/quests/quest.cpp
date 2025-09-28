#include <iostream>
#include <vector>
#include <string>
#include <quests/quest.h>
#include <set>
#include <functional>
#include <bits/stdc++.h>
#include <files/basic_header.h>


bool quest::has_loop() 
{
    std::set<quest*> visited;
    std::set<quest*> recursionStack;

    std::function<bool(quest*)> dfs;
    dfs = [&](quest* current) -> bool {
        if(recursionStack.find(current) != recursionStack.end())
        {
            return true;
        }
        
        if(visited.find(current) != visited.end())
        {
            return false;
        }
        
        visited.insert(current);
        recursionStack.insert(current);

        for(connected_nodes& conn : current->linked_nodes) 
        {
            quest& next_node = *conn.node_linked;
            if (dfs(&next_node)) return true;
        }
        recursionStack.erase(current);
        return false;
    };

    return dfs(this);
}

bool quest::hasNode(quest n)
{
    for(connected_nodes& n1 : this->linked_nodes)
    {
        quest& n2 = *n1.node_linked;
        if(n2 == n) return true;
    }
    return false;
}

bool quest::addConnection(connected_nodes n)
{
    size_t size = this->linked_nodes.size();
    this->linked_nodes.push_back(n);
    if(this->has_loop())
    {
        this->linked_nodes.pop_back();
    }
    else
    {
        n.node_linked->needs_node_completed.push_back(this->id);
    }
    return this->linked_nodes.size() > size ? true : false;
}


static std::string shapeToString(Shapes s) {
    switch(s) {
        case Shapes::CIRCLE:    return "circle";
        case Shapes::NO_SHAPE:  return "no_shape";
        case Shapes::SQUARE:   return "square";
        default:        return "default"; 
    }
}

static std::string progressionToString(Progression p)
{
    return (p == Progression::LINEAR) ? "linear" : "default";
}

std::string quest::generateQuest()
{
    /*std::stringstream ss;
    const std::string indent0(4, ' ');
    const std::string indent1(8, ' ');
    const std::string indent2(12, ' ');

    ss << "{\n";

    // 1. Icono (si existe)
    if (!icon.empty()) {
        ss << indent0 << "icon: {\n";
        ss << indent1 << "id: \"" << icon << "\"\n";
        ss << indent0 << "}\n";
    }

    // 2. ID (siempre presente)
    ss << indent0 << "id: \"" << id << "\"\n";

    auto rewardsList = getRewards();
    if(!rewardsList.empty())
    {
        ss << indent0 << "rewards: [\n";
        for(auto r : rewardsList)
        {
            // Llamada correcta al método generateReward() de cada objeto reward
            for(auto c : r.getRewards())
            {
                std::string rewardStr = r.generateReward(c);
            
                // Procesar cada línea de la recompensa generada
                std::istringstream rs(rewardStr);
                std::string line;
                while(std::getline(rs, line))
                {
                    if(!line.empty())
                    {
                        ss << indent1 << line << "\n";
                    }
                }
            }
        }
        ss << indent0 << "]\n";
    }

    // 4. Forma (si no es DEFAULT)
    if(shape != Shapes::DEFAULT)
    {
        ss << indent0 << "shape: \"" << shapeToString(shape) << "\"\n";
    }

    // 5. Tamaño (si es diferente de 0)
    if(size != 0.0f)
    {
        ss << indent0 << "size: " << std::fixed << std::setprecision(1) << size << "d\n";
    }

    // 6. Dependencias ocultas
    if(hide_until_deps_completed)
    {
        ss << indent0 << "hide_until_deps_complete: true\n";
    }

    // 7. Opcional
    if(optional)
    {
        ss << indent0 << "optional: true\n";
    }

    // 8. Modo de progresión
    if(progression != Progression::DEFAULT)
    {
        ss << indent0 << "progression_mode: \"" << progressionToString(progression) << "\"\n";
    }

    // 9. Tareas (requirements)
    auto tasks = getTasks();
    if(!tasks.empty())
    {
        ss << indent0 << "tasks: [\n";
        for(auto t : requirements)
        {
            ss << indent1 << "{\n";
            
            // ID de la tarea
            ss << indent2 << "id: \"" << t.quest_id << "\"\n";
            
            // Tipo de tarea
            ss << indent2 << "type: \"" << typetoString(t.type) << "\"\n";
            
            // Item (si es tipo "item")
            if(t.type == TYPE::ITEM)
            {
                ss << indent2 << "item: { ";
                ss << "count: " << t.items.count;
                if(!t.items.id.empty())
                {
                    ss << ", id: \"" << t.items.id << "\"";
                }
                ss << " }\n";
            }
            ss << indent1 << "}\n";
        }
        ss << indent0 << "]\n";
    }

    // 10. Dependencias (nodos requeridos)
    if(!needs_node_completed.empty())
    {
        ss << indent0 << "dependencies: [\n";
        for(auto dep : needs_node_completed)
        {
            ss << indent1 << "\"" << dep << "\"\n";
        }
        ss << indent0 << "]\n";
    }

    // 11. Posición (siempre presente)
    vec2 pos = getPosition();
    ss << indent0 << "x: " << pos.x << ".0d\n";
    ss << indent0 << "y: " << pos.y << ".0d\n";

    ss << "}";
    return ss.str()*/
    return "hola";
}