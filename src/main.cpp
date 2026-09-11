#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include "json.hpp"
#include <SFML/System/Angle.hpp>
#include <queue>
#include <unordered_map>
#include <limits>
#include <cmath>

// Hash function for sf::Vector2f
namespace std {
    template<>
    struct hash<sf::Vector2f> {
        size_t operator()(const sf::Vector2f& v) const {
            // Combine hashes of x and y components
            size_t h1 = hash<float>()(v.x);
            size_t h2 = hash<float>()(v.y);
            return h1 ^ (h2 << 1);
        }
    };
}

struct Node {
    sf::Vector2f position;
    bool isDestination;
};

// Edge structure
struct Edge {
    sf::Vector2f from;
    sf::Vector2f to;
};

enum class Mode {
    Idle,
    AddNode,
    RemoveNode,
    AddEdge,
    RemoveEdge,
    FindPath
};

// Add this function before main()
void saveToFile(const std::vector<Node>& destinationNodes, 
                const std::vector<Node>& roadNodes, 
                const std::vector<Edge>& edges) {
    nlohmann::json j;
    j["destinations"] = nlohmann::json::array();
    for (const auto& node : destinationNodes) {
        j["destinations"].push_back({node.position.x, node.position.y});
    }
    j["roads"] = nlohmann::json::array();
    for (const auto& node : roadNodes) {
        j["roads"].push_back({node.position.x, node.position.y});
    }
    j["edges"] = nlohmann::json::array();
    for (const auto& edge : edges) {
        j["edges"].push_back({{edge.from.x, edge.from.y}, {edge.to.x, edge.to.y}});
    }
    std::ofstream outFile("nodes.json");
    outFile << j.dump(4);
}

void centerText(sf::Text& text, unsigned int windowWidth, unsigned int yOffset) {
    sf::FloatRect textBounds = text.getLocalBounds();
    sf::Vector2f pos = textBounds.position;
    sf::Vector2f size = textBounds.size;

    text.setOrigin(sf::Vector2f(pos.x + size.x / 2.0f, pos.y + size.y / 2.0f));
    text.setPosition(sf::Vector2f(windowWidth / 2.0f, yOffset));
}

int findPathNode1 = -1, findPathNode2 = -1;
std::vector<sf::Vector2f> foundPath;

float euclidean(const sf::Vector2f& a, const sf::Vector2f& b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx*dx + dy*dy);
}

std::vector<sf::Vector2f> findShortestPath(
    const sf::Vector2f& start,
    const sf::Vector2f& goal,
    const std::vector<Node>& destinationNodes,
    const std::vector<Node>& roadNodes,
    const std::vector<Edge>& edges
) {
    // Build adjacency list
    std::vector<sf::Vector2f> allNodes;
    for (const auto& n : destinationNodes) allNodes.push_back(n.position);
    for (const auto& n : roadNodes) allNodes.push_back(n.position);

    std::unordered_map<sf::Vector2f, std::vector<sf::Vector2f>> adj;
    for (const auto& node : allNodes) adj[node] = {};
    for (const auto& e : edges) {
        adj[e.from].push_back(e.to);
        adj[e.to].push_back(e.from);
    }

    // Dijkstra
    std::unordered_map<sf::Vector2f, float> dist;
    std::unordered_map<sf::Vector2f, sf::Vector2f> prev;
    auto vec_hash = [](const sf::Vector2f& v) {
        return std::hash<float>()(v.x) ^ std::hash<float>()(v.y);
    };
    auto cmp = [&](const sf::Vector2f& a, const sf::Vector2f& b) {
        return dist[a] > dist[b];
    };
    std::priority_queue<sf::Vector2f, std::vector<sf::Vector2f>, decltype(cmp)> pq(cmp);

    for (const auto& node : allNodes) dist[node] = std::numeric_limits<float>::infinity();
    dist[start] = 0;
    pq.push(start);

    while (!pq.empty()) {
        sf::Vector2f u = pq.top(); pq.pop();
        if (u == goal) break;
        for (const auto& v : adj[u]) {
            float alt = dist[u] + euclidean(u, v);
            if (alt < dist[v]) {
                dist[v] = alt;
                prev[v] = u;
                pq.push(v);
            }
        }
    }

    // Reconstruct path
    std::vector<sf::Vector2f> path;
    if (prev.find(goal) == prev.end()) return path; // No path
    for (sf::Vector2f at = goal; at != start; at = prev[at])
        path.push_back(at);
    path.push_back(start);
    std::reverse(path.begin(), path.end());
    return path;
}

int main()
{
    // Calculate scaled dimensions to fit 1920x1080 screen
    // Using 80% of screen height to leave some margin
    const float scale = (1080.0f * 0.8f) / 1479.0f;
    const unsigned int windowWidth = static_cast<unsigned int>(1479 * scale);
    const unsigned int windowHeight = static_cast<unsigned int>(1479 * scale);
    // const float scale = 1.0f;
    // const unsigned int windowWidth = static_cast<unsigned int>(809 * scale);
    // const unsigned int windowHeight = static_cast<unsigned int>(877 * scale);

    // Create window with scaled dimensions
    auto window = sf::RenderWindow(sf::VideoMode({windowWidth, windowHeight}), "Path Finder");
    window.setFramerateLimit(144);

    // Set window icon
    sf::Image icon;
    if (icon.loadFromFile("path_finder_logo.png")) {
        window.setIcon(icon);
    }

    // Get the screen dimensions
    auto desktop = sf::VideoMode::getDesktopMode();
    
    // Calculate the center position
    int centerX = (desktop.size.x - window.getSize().x) / 2;
    int centerY = (desktop.size.y - window.getSize().y) / 2;
    
    // Set the window position to center
    window.setPosition(sf::Vector2i(centerX, centerY));

    // Load the map texture
    sf::Texture mapTexture;
    // if (!mapTexture.loadFromFile("map2.jpg"))
    if (!mapTexture.loadFromFile("map.png"))
    {
        return -1; // Exit if image loading fails
    }

    // Create a sprite to display the texture
    sf::Sprite mapSprite(mapTexture);
    
    // Scale the sprite to fit the window
    mapSprite.setScale(sf::Vector2f(scale, scale));

    // Create button (Add Node)
    sf::RectangleShape button(sf::Vector2f(175, 40));
    button.setPosition(sf::Vector2f(10, 10));
    button.setFillColor(sf::Color(100, 100, 100));
    button.setOutlineThickness(2);
    button.setOutlineColor(sf::Color::White);

    // Create button text (Add Node)
    sf::Font font;
    if (!font.openFromFile("OpenSans-Regular.ttf")) {
        return -1; // Exit if font loading fails
    }
    sf::Text buttonText(font, "Add Node (A)", 20);
    buttonText.setFillColor(sf::Color::White);
    buttonText.setPosition(sf::Vector2f(20, 15));

    // Node type selection buttons (hidden by default)
    sf::RectangleShape destButton(sf::Vector2f(155, 35));
    destButton.setPosition(sf::Vector2f(20, 60));  // Keep under Add Node
    destButton.setFillColor(sf::Color(200, 80, 80));
    destButton.setOutlineThickness(2);
    destButton.setOutlineColor(sf::Color::White);
    sf::Text destText(font, "Destination (D)", 18);
    destText.setFillColor(sf::Color::White);
    destText.setPosition(sf::Vector2f(30, 65));

    sf::RectangleShape roadButton(sf::Vector2f(155, 35));
    roadButton.setPosition(sf::Vector2f(20, 105));  // Keep under Add Node
    roadButton.setFillColor(sf::Color(80, 80, 200));
    roadButton.setOutlineThickness(2);
    roadButton.setOutlineColor(sf::Color::White);
    sf::Text roadText(font, "Road (F)", 18);
    roadText.setFillColor(sf::Color::White);
    roadText.setPosition(sf::Vector2f(30, 110));

    // Remove Node button
    sf::RectangleShape removeButton(sf::Vector2f(175, 40));
    removeButton.setPosition(sf::Vector2f(195, 10));  // Moved horizontally
    removeButton.setFillColor(sf::Color(120, 40, 40));
    removeButton.setOutlineThickness(2);
    removeButton.setOutlineColor(sf::Color::White);
    sf::Text removeText(font, "Remove Node (R)", 18);
    removeText.setFillColor(sf::Color::White);
    removeText.setPosition(sf::Vector2f(205, 15));

    // Add Edge button
    sf::RectangleShape addEdgeButton(sf::Vector2f(150, 40));
    addEdgeButton.setPosition(sf::Vector2f(380, 10));  // Moved horizontally
    addEdgeButton.setFillColor(sf::Color(40, 120, 40));
    addEdgeButton.setOutlineThickness(2);
    addEdgeButton.setOutlineColor(sf::Color::White);
    sf::Text addEdgeText(font, "Add Edge (E)", 18);
    addEdgeText.setFillColor(sf::Color::White);
    addEdgeText.setPosition(sf::Vector2f(390, 15));

    // Remove Edge button
    sf::RectangleShape removeEdgeButton(sf::Vector2f(175, 40));
    removeEdgeButton.setPosition(sf::Vector2f(540, 10));  // Moved horizontally
    removeEdgeButton.setFillColor(sf::Color(200, 120, 40));
    removeEdgeButton.setOutlineThickness(2);
    removeEdgeButton.setOutlineColor(sf::Color::White);
    sf::Text removeEdgeText(font, "Remove Edge (X)", 18);
    removeEdgeText.setFillColor(sf::Color::White);
    removeEdgeText.setPosition(sf::Vector2f(550, 15));

    // Mode message text - moved to the right of the buttons
    sf::Text modeText(font, "", 26);
    modeText.setFillColor(sf::Color::White);
    modeText.setOutlineColor(sf::Color::Black);
    modeText.setOutlineThickness(2);
    modeText.setPosition(sf::Vector2f(300, 65));

    // Node vectors
    std::vector<Node> destinationNodes;
    std::vector<Node> roadNodes;
    std::vector<Edge> edges;

    // Node selection state
    Mode currentMode = Mode::Idle;
    bool isDestinationNode = false;
    bool showTypeButtons = false;
    int selectedNodeType = -1; // 0: destination, 1: road
    int selectedNodeIndex = -1;
    int removeEdgeNodeType = -1;
    int removeEdgeNodeIndex = -1;

    // For hover effect
    int hoveredNodeType = -1; // 0: destination, 1: road
    int hoveredNodeIndex = -1;

    // Load nodes from file
    {
        std::ifstream inFile("nodes.json");
        if (inFile) {
            nlohmann::json j;
            inFile >> j;
            if (j.contains("destinations")) {
                for (const auto& node : j["destinations"]) {
                    destinationNodes.push_back(Node{sf::Vector2f(node[0], node[1]), true});
                }
            }
            if (j.contains("roads")) {
                for (const auto& node : j["roads"]) {
                    roadNodes.push_back(Node{sf::Vector2f(node[0], node[1]), false});
                }
            }
            if (j.contains("edges")) {
                for (const auto& edge : j["edges"]) {
                    edges.push_back(Edge{sf::Vector2f(edge[0][0], edge[0][1]), sf::Vector2f(edge[1][0], edge[1][1])});
                }
            }
        }
    }

    // Add Find Path button
    sf::RectangleShape findPathButton(sf::Vector2f(150, 40));
    findPathButton.setPosition(sf::Vector2f(725, 10));
    findPathButton.setFillColor(sf::Color(80, 180, 80));
    findPathButton.setOutlineThickness(2);
    findPathButton.setOutlineColor(sf::Color::White);
    sf::Text findPathText(font, "Find Path (P)", 18);
    findPathText.setFillColor(sf::Color::White);
    findPathText.setPosition(sf::Vector2f(735, 15));

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>() || 
                (event->is<sf::Event::KeyPressed>() && 
                 event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape))
            {
                saveToFile(destinationNodes, roadNodes, edges);
                window.close();
                return 0;
            }
            else if (event->is<sf::Event::KeyPressed>()) {
                if (const auto* keyEvent = event->getIf<sf::Event::KeyPressed>()) {
                    sf::Keyboard::Key key = keyEvent->code;
                    switch (key) {
                        case sf::Keyboard::Key::A:
                            showTypeButtons = !showTypeButtons;
                            currentMode = Mode::Idle;
                            foundPath.clear();
                            break;
                        case sf::Keyboard::Key::D:
                            if (showTypeButtons) {
                                currentMode = Mode::AddNode;
                                isDestinationNode = true;
                                showTypeButtons = false;
                                foundPath.clear();
                            }
                            break;
                        case sf::Keyboard::Key::F:
                            if (showTypeButtons) {
                                currentMode = Mode::AddNode;
                                isDestinationNode = false;
                                showTypeButtons = false;
                                foundPath.clear();
                            }
                            break;
                        case sf::Keyboard::Key::R:
                            currentMode = (currentMode == Mode::RemoveNode) ? Mode::Idle : Mode::RemoveNode;
                            showTypeButtons = false;
                            foundPath.clear();
                            break;
                        case sf::Keyboard::Key::E:
                            currentMode = (currentMode == Mode::AddEdge) ? Mode::Idle : Mode::AddEdge;
                            selectedNodeType = -1;
                            selectedNodeIndex = -1;
                            showTypeButtons = false;
                            foundPath.clear();
                            break;
                        case sf::Keyboard::Key::X:
                            currentMode = (currentMode == Mode::RemoveEdge) ? Mode::Idle : Mode::RemoveEdge;
                            removeEdgeNodeType = -1;
                            removeEdgeNodeIndex = -1;
                            showTypeButtons = false;
                            foundPath.clear();
                            break;
                        case sf::Keyboard::Key::P:
                            currentMode = (currentMode == Mode::FindPath) ? Mode::Idle : Mode::FindPath;
                            findPathNode1 = -1;
                            findPathNode2 = -1;
                            showTypeButtons = false;
                            foundPath.clear();
                            break;
                        default:
                            break;
                    }
                }
            }
            else if (event->is<sf::Event::MouseButtonPressed>())
            {
                auto mousePos = sf::Mouse::getPosition(window);

                // Check if button was clicked
                if (button.getGlobalBounds().contains(sf::Vector2f(mousePos)))
                {
                    showTypeButtons = !showTypeButtons;
                    foundPath.clear();
                    if (showTypeButtons) {
                        currentMode = Mode::Idle;
                    } else {
                        currentMode = Mode::Idle;
                    }
                }
                // Check if destination button was clicked
                else if (showTypeButtons && destButton.getGlobalBounds().contains(sf::Vector2f(mousePos)))
                {
                    currentMode = Mode::AddNode;
                    isDestinationNode = true;
                    showTypeButtons = false;
                    foundPath.clear();
                }
                // Check if road button was clicked
                else if (showTypeButtons && roadButton.getGlobalBounds().contains(sf::Vector2f(mousePos)))
                {
                    currentMode = Mode::AddNode;
                    isDestinationNode = false;
                    showTypeButtons = false;
                    foundPath.clear();
                }
                // Check if remove button was clicked
                else if (removeButton.getGlobalBounds().contains(sf::Vector2f(mousePos)))
                {
                    currentMode = (currentMode == Mode::RemoveNode) ? Mode::Idle : Mode::RemoveNode;
                    showTypeButtons = false;
                }
                // Check if add edge button was clicked
                else if (addEdgeButton.getGlobalBounds().contains(sf::Vector2f(mousePos)))
                {
                    currentMode = (currentMode == Mode::AddEdge) ? Mode::Idle : Mode::AddEdge;
                    selectedNodeType = -1;
                    selectedNodeIndex = -1;
                    showTypeButtons = false;
                    foundPath.clear();
                }
                // Check if remove edge button was clicked
                else if (removeEdgeButton.getGlobalBounds().contains(sf::Vector2f(mousePos)))
                {
                    currentMode = (currentMode == Mode::RemoveEdge) ? Mode::Idle : Mode::RemoveEdge;
                    removeEdgeNodeType = -1;
                    removeEdgeNodeIndex = -1;
                    showTypeButtons = false;
                    foundPath.clear();
                }
                // Check if find path button was clicked
                else if (findPathButton.getGlobalBounds().contains(sf::Vector2f(mousePos)))
                {
                    currentMode = (currentMode == Mode::FindPath) ? Mode::Idle : Mode::FindPath;
                    findPathNode1 = -1;
                    findPathNode2 = -1;
                    showTypeButtons = false;
                    foundPath.clear();
                }
                // Manual edge adding mode
                else if (currentMode == Mode::AddEdge && (hoveredNodeType != -1 && hoveredNodeIndex != -1))
                {
                    if (selectedNodeType == -1) {
                        // First node selected
                        selectedNodeType = hoveredNodeType;
                        selectedNodeIndex = hoveredNodeIndex;
                    } else {
                        // Second node selected, create edge
                        sf::Vector2f from, to;
                        if (selectedNodeType == 0)
                            from = destinationNodes[selectedNodeIndex].position;
                        else
                            from = roadNodes[selectedNodeIndex].position;
                        if (hoveredNodeType == 0)
                            to = destinationNodes[hoveredNodeIndex].position;
                        else
                            to = roadNodes[hoveredNodeIndex].position;
                        edges.push_back({from, to});
                        // Reset selection for next edge
                        selectedNodeType = -1;
                        selectedNodeIndex = -1;
                        currentMode = Mode::Idle;
                    }
                }
                // Manual edge removal mode
                else if (currentMode == Mode::RemoveEdge && (hoveredNodeType != -1 && hoveredNodeIndex != -1))
                {
                    if (removeEdgeNodeType == -1) {
                        // First node selected
                        removeEdgeNodeType = hoveredNodeType;
                        removeEdgeNodeIndex = hoveredNodeIndex;
                    } else {
                        // Second node selected, try to remove edge
                        sf::Vector2f from, to;
                        if (removeEdgeNodeType == 0)
                            from = destinationNodes[removeEdgeNodeIndex].position;
                        else
                            from = roadNodes[removeEdgeNodeIndex].position;
                        if (hoveredNodeType == 0)
                            to = destinationNodes[hoveredNodeIndex].position;
                        else
                            to = roadNodes[hoveredNodeIndex].position;
                        // Remove edge if it exists (in either direction)
                        for (auto it = edges.begin(); it != edges.end(); ++it) {
                            if ((it->from == from && it->to == to) || (it->from == to && it->to == from)) {
                                edges.erase(it);
                                break;
                            }
                        }
                        // Reset selection for next removal
                        removeEdgeNodeType = -1;
                        removeEdgeNodeIndex = -1;
                        currentMode = Mode::Idle;
                    }
                }
                // Remove node mode
                else if (currentMode == Mode::RemoveNode)
                {
                    // Check destination nodes
                    for (auto it = destinationNodes.begin(); it != destinationNodes.end(); ++it) {
                        sf::CircleShape nodeShape(5);
                        nodeShape.setPosition(it->position);
                        if (nodeShape.getGlobalBounds().contains(window.mapPixelToCoords(mousePos))) {
                            sf::Vector2f removedPos = it->position;
                            destinationNodes.erase(it);
                            // Remove all edges connected to this node
                            edges.erase(std::remove_if(edges.begin(), edges.end(), [&](const Edge& e) {
                                return e.from == removedPos || e.to == removedPos;
                            }), edges.end());
                            currentMode = Mode::Idle;
                            break;
                        }
                    }
                    // Check road nodes if not already removed
                    if (currentMode != Mode::RemoveNode) continue;
                    for (auto it = roadNodes.begin(); it != roadNodes.end(); ++it) {
                        sf::CircleShape nodeShape(5);
                        nodeShape.setPosition(it->position);
                        if (nodeShape.getGlobalBounds().contains(window.mapPixelToCoords(mousePos))) {
                            sf::Vector2f removedPos = it->position;
                            roadNodes.erase(it);
                            // Remove all edges connected to this node
                            edges.erase(std::remove_if(edges.begin(), edges.end(), [&](const Edge& e) {
                                return e.from == removedPos || e.to == removedPos;
                            }), edges.end());
                            currentMode = Mode::Idle;
                            break;
                        }
                    }
                }
                // Add node mode
                else if (currentMode == Mode::AddNode && mousePos.x > 0 && mousePos.x < windowWidth &&
                         mousePos.y > 0 && mousePos.y < windowHeight)
                {
                    // Convert mouse position to world coordinates
                    sf::Vector2f worldPos = window.mapPixelToCoords(mousePos);
                    // Create and add the node
                    Node newNode;
                    newNode.position = worldPos;
                    newNode.isDestination = isDestinationNode;
                    if (isDestinationNode) {
                        destinationNodes.push_back(newNode);
                    } else {
                        roadNodes.push_back(newNode);
                    }
                    currentMode = Mode::Idle;
                }
                // Find path mode
                else if (currentMode == Mode::FindPath && hoveredNodeType == 0 && hoveredNodeIndex != -1)
                {
                    if (findPathNode1 == -1) {
                        findPathNode1 = hoveredNodeIndex;
                    } else if (findPathNode2 == -1 && hoveredNodeIndex != findPathNode1) {
                        findPathNode2 = hoveredNodeIndex;
                        // Run pathfinding here!
                        foundPath = findShortestPath(
                            destinationNodes[findPathNode1].position,
                            destinationNodes[findPathNode2].position,
                            destinationNodes, roadNodes, edges
                        );
                        currentMode = Mode::Idle;
                        findPathNode1 = -1;
                        findPathNode2 = -1;
                    }
                }
            }
        }

        window.clear();
        window.draw(mapSprite); // Draw the map

        // Draw edges (thick lines)
        for (const auto& edge : edges) {
            sf::Vector2f diff = edge.to - edge.from;
            float length = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            float angle = std::atan2(diff.y, diff.x) * 180 / 3.14159265f;
            sf::RectangleShape thickLine(sf::Vector2f(length, 5)); // 5 pixels thick
            thickLine.setPosition(edge.from);
            thickLine.setFillColor(sf::Color::Yellow);
            thickLine.setRotation(sf::degrees(angle));
            window.draw(thickLine);
        }
        
        // --- HOVER LOGIC ---
        hoveredNodeType = -1;
        hoveredNodeIndex = -1;
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        sf::Vector2f mouseWorld = window.mapPixelToCoords(mousePos);
        // Check destination nodes
        for (size_t i = 0; i < destinationNodes.size(); ++i) {
            sf::CircleShape nodeShape(5);
            nodeShape.setPosition(destinationNodes[i].position);
            if (nodeShape.getGlobalBounds().contains(mouseWorld)) {
                hoveredNodeType = 0;
                hoveredNodeIndex = i;
                break;
            }
        }

        // Check road nodes if not already hovering
        if (hoveredNodeType == -1) {
            for (size_t i = 0; i < roadNodes.size(); ++i) {
                sf::CircleShape nodeShape(5);
                nodeShape.setPosition(roadNodes[i].position);
                if (nodeShape.getGlobalBounds().contains(mouseWorld)) {
                    hoveredNodeType = 1;
                    hoveredNodeIndex = i;
                    break;
                }
            }
        }

        // Draw nodes
        for (const auto& node : destinationNodes) {
            sf::CircleShape nodeShape(5);
            nodeShape.setPosition(node.position);
            nodeShape.setFillColor(sf::Color::Red);
            window.draw(nodeShape);
        }
        
        for (const auto& node : roadNodes) {
            sf::CircleShape nodeShape(5);
            nodeShape.setPosition(node.position);
            nodeShape.setFillColor(sf::Color::Blue);
            window.draw(nodeShape);
        }

        // Draw hover effect
        if (hoveredNodeType != -1 && hoveredNodeIndex != -1) {
            sf::CircleShape hoverShape(8);
            if (hoveredNodeType == 0) {
                hoverShape.setPosition(destinationNodes[hoveredNodeIndex].position - sf::Vector2f(3, 3));
                hoverShape.setOutlineColor(sf::Color::Red);
            } else {
                hoverShape.setPosition(roadNodes[hoveredNodeIndex].position - sf::Vector2f(3, 3));
                hoverShape.setOutlineColor(sf::Color::Blue);
            }
            hoverShape.setFillColor(sf::Color::Transparent);
            hoverShape.setOutlineThickness(3);
            window.draw(hoverShape);
        }

        // Draw selection highlight for manual edge
        if (currentMode == Mode::AddEdge && selectedNodeType != -1 && selectedNodeIndex != -1) {
            sf::Vector2f pos = (selectedNodeType == 0) ? destinationNodes[selectedNodeIndex].position : roadNodes[selectedNodeIndex].position;
            sf::CircleShape selShape(10);
            selShape.setPosition(pos - sf::Vector2f(5, 5));
            selShape.setFillColor(sf::Color::Transparent);
            selShape.setOutlineColor(sf::Color::Green);
            selShape.setOutlineThickness(3);
            window.draw(selShape);
        }
        // Draw selection highlight for manual edge removal
        if (currentMode == Mode::RemoveEdge && removeEdgeNodeType != -1 && removeEdgeNodeIndex != -1) {
            sf::Vector2f pos = (removeEdgeNodeType == 0) ? destinationNodes[removeEdgeNodeIndex].position : roadNodes[removeEdgeNodeIndex].position;
            sf::CircleShape selShape(10);
            selShape.setPosition(pos - sf::Vector2f(5, 5));
            selShape.setFillColor(sf::Color::Transparent);
            selShape.setOutlineColor(sf::Color::Magenta);
            selShape.setOutlineThickness(3);
            window.draw(selShape);
        }

        // Draw selection highlight for find path
        if (currentMode == Mode::FindPath && findPathNode1 != -1) {
            sf::Vector2f pos = destinationNodes[findPathNode1].position;
            sf::CircleShape selShape(10);
            selShape.setPosition(pos - sf::Vector2f(5, 5));
            selShape.setFillColor(sf::Color::Transparent);
            selShape.setOutlineColor(sf::Color::Green);
            selShape.setOutlineThickness(3);
            window.draw(selShape);
        }

        // Draw button and text
        window.draw(button);
        window.draw(buttonText);
        if (showTypeButtons) {
            window.draw(destButton);
            window.draw(destText);
            window.draw(roadButton);
            window.draw(roadText);
        }
        window.draw(removeButton);
        window.draw(removeText);
        window.draw(removeEdgeButton);
        window.draw(removeEdgeText);

        // Set and draw mode message
        switch (currentMode) {
            case Mode::RemoveNode:
                modeText.setString("Remove node");
                break;
            case Mode::RemoveEdge:
                if (removeEdgeNodeType == -1)
                    modeText.setString("Select first node (remove edge)");
                else
                    modeText.setString("Select second node (remove edge)");
                break;
            case Mode::AddEdge:
                if (selectedNodeType == -1)
                    modeText.setString("Select first node (add edge)");
                else
                    modeText.setString("Select second node (add edge)");
                break;
            case Mode::AddNode:
                if (isDestinationNode)
                    modeText.setString("Add destination");
                else
                    modeText.setString("Add road");
                break;
            case Mode::FindPath:
                if (findPathNode1 == -1)
                    modeText.setString("Select first node (find path)");
                else if (findPathNode2 == -1)
                    modeText.setString("Select second node (find path)");
                else
                    modeText.setString("Shortest path");
                break;
            case Mode::Idle:
            default:
                if (showTypeButtons)
                    modeText.setString("Node type");
                else
                    modeText.setString("");
                break;
        }
        centerText(modeText, windowWidth, 80);
        window.draw(modeText);
        
        window.draw(addEdgeButton);
        window.draw(addEdgeText);
        
        window.draw(findPathButton);
        window.draw(findPathText);
        
        if (!foundPath.empty()) {
            for (size_t i = 1; i < foundPath.size(); ++i) {
                sf::Vector2f from = foundPath[i-1], to = foundPath[i];
                sf::Vector2f diff = to - from;
                float length = std::sqrt(diff.x * diff.x + diff.y * diff.y);
                float angle = std::atan2(diff.y, diff.x) * 180 / 3.14159265f;
                sf::RectangleShape thickLine(sf::Vector2f(length, 7)); // 7 pixels thick
                thickLine.setPosition(from);
                thickLine.setFillColor(sf::Color::Green);
                thickLine.setRotation(sf::degrees(angle));
                window.draw(thickLine);
            }
        }
        
        window.display();
    }

    // Save before normal program end
    saveToFile(destinationNodes, roadNodes, edges);
    return 0;
}
