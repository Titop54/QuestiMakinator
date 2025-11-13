#include "imgui.h"
#include "imgui-SFML.h"
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/WindowEnums.hpp>
#include <cmath>
#include "imgui_stdlib.h"
#include "parser/raw.h"


#include <SFML/Network.hpp>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

class ProbeJSClient {
private:
    int port;
    std::string auth;
    bool connected;
    sf::TcpSocket socket;

public:
    ProbeJSClient(int port = 61423, std::string auth = "probejs")
        : port(port), auth(auth), connected(false) {}

    bool connect() {
        if (socket.connect(sf::IpAddress::LocalHost, port, sf::seconds(5)) == sf::Socket::Status::Done) {
            connected = true;
            std::cout << "Conectado a ProbeJS en puerto " << port << std::endl;
            return true;
        } else {
            std::cout << "Error conectando a ProbeJS en puerto " << port << std::endl;
            connected = false;
            return false;
        }
    }

    bool tryConnect() {
        int originalPort = port;
        
        // Intentar conectar en varios puertos (como hace la extensión)
        for (int i = 0; i < 10; i++) {
            if (connect()) {
                return true;
            }
            port++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Volver al puerto original y probar una vez más
        port = originalPort;
        return connect();
    }

    bool sendReloadCommand(const std::string& scriptType) {
        if (!connected && !tryConnect()) {
            std::cout << "No se pudo conectar a ProbeJS" << std::endl;
            return false;
        }

        std::string path;
        if (scriptType == "server_scripts") {
            path = "/api/reload/server";
        } else if (scriptType == "client_scripts") {
            path = "/api/reload/client";
        } else if (scriptType == "startup_scripts") {
            path = "/api/reload/startup";
        } else {
            return false;
        }

        // Construir la petición HTTP completa como lo hace axios
        std::stringstream httpRequest;
        httpRequest << "POST " << path << " HTTP/1.1\r\n";
        httpRequest << "Host: localhost:" << port << "\r\n";
        httpRequest << "Accept: application/json, text/plain, */*\r\n";
        httpRequest << "Content-Type: application/x-www-form-urlencoded\r\n";
        httpRequest << "Authorization: Bearer " << auth << "\r\n";
        httpRequest << "User-Agent: MyCppApp/1.0\r\n";
        httpRequest << "Connection: close\r\n";
        httpRequest << "Content-Length: 0\r\n";
        httpRequest << "\r\n";

        std::string requestStr = httpRequest.str();
        
        if (socket.send(requestStr.c_str(), requestStr.size()) != sf::Socket::Status::Done) {
            std::cout << "Error enviando comando reload" << std::endl;
            connected = false;
            return false;
        }

        std::cout << "Comando reload enviado: " << scriptType << std::endl;

        // Esperar y leer la respuesta
        char buffer[4096];
        std::size_t received;
        std::string response;
        
        // Leer toda la respuesta disponible
        while (socket.receive(buffer, sizeof(buffer), received) == sf::Socket::Status::Done) {
            response.append(buffer, received);
            if (received < sizeof(buffer)) break; // Probablemente llegamos al final
        }

        // Verificar si la respuesta fue exitosa
        if (response.find("HTTP/1.1 200") != std::string::npos || 
            response.find("HTTP/1.1 204") != std::string::npos) {
            std::cout << "Comando ejecutado exitosamente" << std::endl;
            return true;
        } else {
            std::cout << "Error en la respuesta: " << std::endl;
            // Imprimir solo las primeras líneas para debug
            std::istringstream responseStream(response);
            std::string line;
            int lineCount = 0;
            while (std::getline(responseStream, line) && lineCount < 5) {
                std::cout << line << std::endl;
                lineCount++;
            }
            return false;
        }
    }

    bool sendCommand(const std::string& command) {
        if (!connected && !tryConnect()) {
            std::cout << "No se pudo conectar a ProbeJS" << std::endl;
            return false;
        }

        // Construir el cuerpo JSON con el comando
        std::string jsonBody = "{\"command\":\"" + command + "\"}";

        // Construir la petición HTTP para ejecutar comandos
        std::stringstream httpRequest;
        httpRequest << "POST /api/probejs/run-command HTTP/1.1\r\n";
        httpRequest << "Host: localhost:" << port << "\r\n";
        httpRequest << "Accept: application/json, text/plain, */*\r\n";
        httpRequest << "Content-Type: application/json\r\n";
        httpRequest << "Authorization: Bearer " << auth << "\r\n";
        httpRequest << "User-Agent: MyCppApp/1.0\r\n";
        httpRequest << "Connection: close\r\n";
        httpRequest << "Content-Length: " << jsonBody.length() << "\r\n";
        httpRequest << "\r\n";
        httpRequest << jsonBody;

        std::string requestStr = httpRequest.str();
        
        if (socket.send(requestStr.c_str(), requestStr.size()) != sf::Socket::Status::Done) {
            std::cout << "Error enviando comando: " << command << std::endl;
            connected = false;
            return false;
        }

        std::cout << "Comando enviado: " << command << std::endl;

        // Esperar y leer la respuesta
        char buffer[4096];
        std::size_t received;
        std::string response;
        
        // Leer toda la respuesta disponible
        while (socket.receive(buffer, sizeof(buffer), received) == sf::Socket::Status::Done) {
            response.append(buffer, received);
            if (received < sizeof(buffer)) break; // Probablemente llegamos al final
        }

        // Verificar si la respuesta fue exitosa
        if (response.find("HTTP/1.1 200") != std::string::npos || 
            response.find("HTTP/1.1 204") != std::string::npos) {
            std::cout << "Comando ejecutado exitosamente" << std::endl;
            
            // Extraer y mostrar la respuesta del comando si está disponible
            size_t jsonStart = response.find("{");
            if (jsonStart != std::string::npos) {
                std::string jsonResponse = response.substr(jsonStart);
                std::cout << "Respuesta: " << jsonResponse << std::endl;
            }
            
            return true;
        } else {
            std::cout << "Error en la respuesta del comando: " << std::endl;
            // Imprimir solo las primeras líneas para debug
            std::istringstream responseStream(response);
            std::string line;
            int lineCount = 0;
            while (std::getline(responseStream, line) && lineCount < 10) {
                std::cout << line << std::endl;
                lineCount++;
            }
            return false;
        }
    }

    void disconnect() {
        socket.disconnect();
        connected = false;
    }

    bool isConnected() const {
        return connected;
    }
};

// Estructura para manejar el estado de selección de texto
struct TextEditorState {
    std::string text;
    bool hasSelection = false;
    int selectionStart = 0;
    int selectionEnd = 0;
    std::string selectedText;
    
    void updateSelection() {
        if (hasSelection && selectionStart != selectionEnd) {
            int start = std::min(selectionStart, selectionEnd);
            int end = std::max(selectionStart, selectionEnd);
            selectedText = text.substr(start, end - start);
        } else {
            selectedText.clear();
            hasSelection = false;
        }
    }
    
    std::string wrapSelection(const std::string& prefix, const std::string& suffix = "") {
        if (hasSelection && selectionStart != selectionEnd) {
            int start = std::min(selectionStart, selectionEnd);
            int end = std::max(selectionStart, selectionEnd);
            
            std::string result = text;
            result.replace(start, end - start, prefix + selectedText + suffix);
            
            // Actualizar posiciones después del reemplazo
            selectionStart = start + prefix.length();
            selectionEnd = selectionStart + selectedText.length();
            
            return result;
        } else {
            // Si no hay selección, añadir al final
            return text + prefix + suffix;
        }
    }
};

// Función para mostrar tooltips
void ShowHelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// Callback personalizado para capturar la selección
static int InputTextCallback(ImGuiInputTextCallbackData* data) {
    TextEditorState* state = static_cast<TextEditorState*>(data->UserData);
    if (state) {
        state->selectionStart = data->SelectionStart;
        state->selectionEnd = data->SelectionEnd;
        state->hasSelection = (data->SelectionStart != data->SelectionEnd);
        state->updateSelection();
    }
    return 0;
}

int main()
{
    sf::RenderWindow window(
                     sf::VideoMode(sf::Vector2u(800, 600)),
                    "Prueba",
                    sf::Style::Default,
                    sf::State::Windowed,
                    {}
    );

    auto desktop = sf::VideoMode::getDesktopMode();
    int x = desktop.size.x/2 - window.getSize().x/2;
    int y = desktop.size.y/2 - window.getSize().y/2;
    window.setPosition(sf::Vector2(x, y));

    if(!ImGui::SFML::Init(window)) return -1;

    window.setFramerateLimit(60);
    
    // Usar nuestro estado personalizado
    TextEditorState editorState;
    std::string inputText2 = "";
    
    // Variables para efectos con parámetros
    std::string effectParam1 = "1.0";
    std::string effectParam2 = "1.0";
    std::string effectParam3 = "1.0";
    std::string customText = "";
    std::string colorHex = "FFFFFF";
    
    // Estado para radio buttons
    static int selected_option = 0;

    // After calling init, we need to pass the style
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.1f, 0.9f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.5f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.6f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.7f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.WindowRounding = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.FrameRounding = 0.0f;

    float zoom_factor = 1.0f;
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = zoom_factor;
    sf::Clock deltaClock;

    while(window.isOpen())
    {
        while(std::optional<sf::Event> event = window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(window, *event);
            if(event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());
        ImVec2 center = ImVec2(window.getSize().x / 2.0f, window.getSize().y / 2.0f);
        ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

        ImGui::Begin("Window");
        float width = ImGui::GetContentRegionAvail().x;

        // ===== GRUPO 1: FORMATOS BÁSICOS =====
        ImGui::Text("Formatos Básicos:");
        
        if (ImGui::Button("Bold")) {
            editorState.text = editorState.wrapSelection("&l");
        }
        ImGui::SameLine();
        ShowHelpMarker("Texto en negrita (&l)");
        
        if (ImGui::Button("Italic")) {
            editorState.text = editorState.wrapSelection("&o");
        }
        ImGui::SameLine();
        ShowHelpMarker("Texto en cursiva (&o)");
        
        if (ImGui::Button("Underline")) {
            editorState.text = editorState.wrapSelection("&n");
        }
        ImGui::SameLine();
        ShowHelpMarker("Texto subrayado (&n)");
        
        if (ImGui::Button("Strikethrough")) {
            editorState.text = editorState.wrapSelection("&m");
        }
        ImGui::SameLine();
        ShowHelpMarker("Texto tachado (&m)");
        
        if (ImGui::Button("Reset")) {
            editorState.text = editorState.wrapSelection("&r");
        }
        ImGui::SameLine();
        ShowHelpMarker("Resetear formato (&r)");
        
        if (ImGui::Button("Obfuscated")) {
            editorState.text = editorState.wrapSelection("&k");
        }
        ImGui::SameLine();
        ShowHelpMarker("Texto ofuscado (&k)");

        ImGui::NewLine();

        // ===== GRUPO 2: COLORES BÁSICOS =====
        ImGui::Text("Colores Básicos:");
        
        // Primera fila de colores
        if (ImGui::Button("Negro (&0)")) {
            editorState.text = editorState.wrapSelection("&0");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Azul Osc (&1)")) {
            editorState.text = editorState.wrapSelection("&1");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Verde Osc (&2)")) {
            editorState.text = editorState.wrapSelection("&2");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Azul Claro (&3)")) {
            editorState.text = editorState.wrapSelection("&3");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Rojo (&4)")) {
            editorState.text = editorState.wrapSelection("&4");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Púrpura (&5)")) {
            editorState.text = editorState.wrapSelection("&5");
        }

        // Segunda fila de colores
        if (ImGui::Button("Naranja (&6)")) {
            editorState.text = editorState.wrapSelection("&6");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Gris Claro (&7)")) {
            editorState.text = editorState.wrapSelection("&7");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Gris (&8)")) {
            editorState.text = editorState.wrapSelection("&8");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Azul (&9)")) {
            editorState.text = editorState.wrapSelection("&9");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Verde Claro (&a)")) {
            editorState.text = editorState.wrapSelection("&a");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Agua (&b)")) {
            editorState.text = editorState.wrapSelection("&b");
        }

        // Tercera fila de colores
        if (ImGui::Button("Rojo Claro (&c)")) {
            editorState.text = editorState.wrapSelection("&c");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Rosa (&d)")) {
            editorState.text = editorState.wrapSelection("&d");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Amarillo (&e)")) {
            editorState.text = editorState.wrapSelection("&e");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Blanco (&f)")) {
            editorState.text = editorState.wrapSelection("&f");
        }

        ImGui::NewLine();

        // ===== GRUPO 3: ACCIONES ESPECIALES =====
        ImGui::Text("Acciones Especiales:");
        
        if (ImGui::Button("Insert URL")) {
            editorState.text = editorState.wrapSelection("&@url:\"", "\"");
        }
        ImGui::SameLine();
        ShowHelpMarker("Insertar URL: &@url:\"url\"");
        
        if (ImGui::Button("Insert Text")) {
            editorState.text = editorState.wrapSelection("&@in:\"", "\"");
        }
        ImGui::SameLine();
        ShowHelpMarker("Insertar texto de chat: &@in:\"texto\"");
        
        if (ImGui::Button("Open File")) {
            editorState.text = editorState.wrapSelection("&@file:\"", "\"");
        }
        ImGui::SameLine();
        ShowHelpMarker("Abrir archivo: &@file:\"ruta\"");
        
        if (ImGui::Button("Run Command")) {
            editorState.text = editorState.wrapSelection("&@command:\"", "\"");
        }
        ImGui::SameLine();
        ShowHelpMarker("Ejecutar comando: &@command:\"comando\"");
        
        if (ImGui::Button("Copy Text")) {
            editorState.text = editorState.wrapSelection("&@copy:\"", "\"");
        }
        ImGui::SameLine();
        ShowHelpMarker("Copiar texto: &@copy:\"texto\"");
        
        if (ImGui::Button("Change Quest")) {
            editorState.text = editorState.wrapSelection("&@change:\"", "\"");
        }
        ImGui::SameLine();
        ShowHelpMarker("Cambiar misión: &@change:\"texto\"");
        
        if (ImGui::Button("New Page")) {
            editorState.text = editorState.wrapSelection("&@page");
        }
        ImGui::SameLine();
        ShowHelpMarker("Nueva página: &@page");

        ImGui::NewLine();

        // ===== GRUPO 4: HOVER EFFECTS =====
        ImGui::Text("Efectos Hover:");
        
        if (ImGui::Button("Show Text Hover")) {
            editorState.text = editorState.wrapSelection("&&text:\"", "\"");
        }
        ImGui::SameLine();
        ShowHelpMarker("Mostrar texto al hover: &&text:\"texto\"");
        
        if (ImGui::Button("Show Item Hover")) {
            editorState.text = editorState.wrapSelection("&&item:\"", "\"");
        }
        ImGui::SameLine();
        ShowHelpMarker("Mostrar item al hover: &&item:\"item\"");

        ImGui::NewLine();

        // ===== GRUPO 5: EFECTOS DEL MOD =====
        ImGui::Text("Efectos del Mod:");
        
        // Primera fila de efectos
        if (ImGui::Button("Typewriter")) {
            editorState.text = editorState.wrapSelection("<typewriter>", "</typewriter>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Efecto máquina de escribir");
        
        if (ImGui::Button("Bounce")) {
            editorState.text = editorState.wrapSelection("<bounce a=1.0 f=1.0 w=1.0>", "</bounce>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Rebote vertical");
        
        if (ImGui::Button("Fade")) {
            editorState.text = editorState.wrapSelection("<fade a=0.3 f=1.0 w=0.0>", "</fade>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Efecto desvanecimiento");
        
        if (ImGui::Button("Glitch")) {
            editorState.text = editorState.wrapSelection("<glitch f=1.0 j=0.015 b=0.003 s=0.08>", "</glitch>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Efecto glitch");

        // Segunda fila de efectos
        if (ImGui::Button("Gradient")) {
            editorState.text = editorState.wrapSelection("<grad from=#7FFFD4 to=#1E90FF hue=false f=0.0 sp=20.0 uni=false>", "</grad>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Gradiente de color");
        
        if (ImGui::Button("Neon")) {
            editorState.text = editorState.wrapSelection("<neon p=10 r=2 a=0.12>", "</neon>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Efecto neón");
        
        if (ImGui::Button("Pendulum")) {
            editorState.text = editorState.wrapSelection("<pend f=1.0 a=30 r=0.0>", "</pend>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Péndulo circular");
        
        if (ImGui::Button("Pulse")) {
            editorState.text = editorState.wrapSelection("<pulse base=0.75 a=1.0 f=1.0 w=0.0>", "</pulse>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Pulsación de brillo");

        // Tercera fila de efectos
        if (ImGui::Button("Rainbow")) {
            editorState.text = editorState.wrapSelection("<rainb f=1.0 w=1.0>", "</rainb>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Efecto arcoíris");
        
        if (ImGui::Button("Shadow")) {
            editorState.text = editorState.wrapSelection("<shadow x=0.0 y=0.0 c=000000 a=1.0>", "</shadow>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Modificar sombra");
        
        if (ImGui::Button("Shake")) {
            editorState.text = editorState.wrapSelection("<shake a=1.0 f=1.0>", "</shake>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Vibración aleatoria");
        
        if (ImGui::Button("Swing")) {
            editorState.text = editorState.wrapSelection("<swing a=1.0 f=1.0 w=0.0>", "</swing>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Oscilación de caracteres");

        // Cuarta fila de efectos
        if (ImGui::Button("Turbulence")) {
            editorState.text = editorState.wrapSelection("<turb a=1.0 f=1.0>", "</turb>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Efecto turbulencia");
        
        if (ImGui::Button("Wave")) {
            editorState.text = editorState.wrapSelection("<wave a=1.0 f=1.0 w=1.0>", "</wave>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Ondulación como olas");
        
        if (ImGui::Button("Wiggle")) {
            editorState.text = editorState.wrapSelection("<wiggle a=1.0 f=1.0 w=1.0>", "</wiggle>");
        }
        ImGui::SameLine();
        ShowHelpMarker("Movimiento aleatorio por caracter");

        // Separador antes de los textfields
        ImGui::Separator();

        // Mostrar información de selección (debug)
        if (editorState.hasSelection) {
            ImGui::Text("Selección: '%s' (%d-%d)", 
                       editorState.selectedText.c_str(), 
                       editorState.selectionStart, 
                       editorState.selectionEnd);
        } else {
            ImGui::Text("Sin selección");
        }

        // Primer textfield con callback para capturar selección
        ImVec2 size(width, ImGui::GetTextLineHeight() * 8);
        ImGui::InputTextMultiline("##input1", &editorState.text, size, 
            ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways,
            InputTextCallback, &editorState);

        // Segundo textfield
        ImGui::InputTextMultiline("##input2", &inputText2, size, 
            ImGuiInputTextFlags_ReadOnly);

        // Radio buttons (solo uno seleccionado)
        ImGui::Text("Opciones:");
        ImGui::RadioButton("Array", &selected_option, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Extra", &selected_option, 1);
        ImGui::SameLine();

        // Botones de acción
        if(ImGui::Button("Convert"))
        {
            inputText2 = raw::to_json(editorState.text, selected_option ? true : false);
            ImGui::SetClipboardText(inputText2.c_str());
        }
        ImGui::SameLine();

        // Botón para copiar texto
        if(ImGui::Button("Copiar Texto"))
        {
            ImGui::SetClipboardText(inputText2.c_str());
        }
        ImGui::SameLine();

        if(ImGui::Button("Reload Minecraft"))
        {
            static ProbeJSClient probe(61423, "1wR7P7aCSu-0p5Yc6zYhzUkYp2y5dNQWI05rZaEYEaYH");
            if(probe.tryConnect())
            {
                if(probe.sendCommand("give @p minecraft:diamond 1")) 
                {
                    inputText2 = "Reload exitoso!";
                }
                else
                {
                    inputText2 = "Error al ejecutar reload";
                }
            }
            else
            {
                inputText2 = "No se ha podido conectar con el server";
            }
        }
        ImGui::SameLine();

        if(ImGui::Button("Exit"))
        {
            window.close();
        }
        
        ImGui::End();

        window.clear();
        ImGui::SFML::Render(window);
        window.display();
    }
    ImGui::SFML::Shutdown();

    return 0;
}