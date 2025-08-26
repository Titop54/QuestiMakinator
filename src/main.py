//#include <iostream>
#include <parser/raw.h>
//#include <SFML/Audio.hpp>
//#include <unistd.h>
//#include <climits>
//#include <filesystem>
//#include <quests/quest.h>
//#include <quests/reward.h>
//#include <iostream>
//#include <quests/uuid.h>
//#include <quests/chapter.h>
//#include <parser/parser.h>
//#include <imgui.h>
//#include <imgui-SFML.h>
//#include <SFML/Graphics/RenderWindow.hpp>

/*reward test();
quest test2();
void test3();
void test4();
void test5();
void test6();*/

/*int main() {
    sf::RenderWindow window(sf::VideoMode(800, 600));
    ImGui::SFML::Init(window);
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.1f, 0.9f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.5f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.6f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.7f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.WindowRounding = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.FrameRounding = 0.0f;

    sf::Clock deltaClock;
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);
            if (event.type == sf::Event::Closed) window.close();
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        // Aquí construyes tu interfaz ImGui
        ImGui::Begin("Ventana");
        ImGui::Text("");
        ImGui::End();

        window.clear();
        ImGui::SFML::Render(window);
        window.display();
    }
    ImGui::SFML::Shutdown();
}*/

int main()
{   
    //std::string json = R"([{"text":"Similar to ","color":"#FFFFFF"},{"text":"Copper Upgrades","color":"#e69429"},{"text":", it gives you more ","color":"#FFFFFF"},{"text":"capacity","color":"#e62962"},{"text":" but at an ","color":"#FFFFFF"},{"text":"higher","color":"#29e69e"},{"text":" cost","color":"#FFFFFF"}])";
    //std::cout << raw::resolve(json) <<std::endl;
    //test3();
    //raw::to_json("&aHello &lWorld&r!");
    raw::interactive_converter();
    
    //test5();
    //test6();
    //auto app = Gtk::Application::create();

    //Shows the window and returns when it is closed.
    //app->make_window_and_run<HelloWorld>(argc, argv);
    return 0;
}


/*void test3() {
    std::ifstream file("quests/example.snbt");
    std::string line;
    std::string file_content;
    while(std::getline(file, line))
    {
        file_content += line;
        file_content.push_back('\n');
    }

    try
    {
        snbt::Parser parser(file_content);
        snbt::Tag tag = parser.parse();

        const auto& comp = tag.as<snbt::Compound>();
        std::ofstream exit("quests/ae2.snbt");
        std::string str;
        str += "{\n";
        for (const auto& [key, value] : comp) {
            str += ("    " + key + ": " + snbt::to_string(value, 1)) + "\n";
        }
        str += "}";
        exit << str << std::endl;
        exit.close();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    return;
}

void test4()
{
    snbt::Compound comp;
    comp["id"] = snbt::Tag("777");
    comp["vegetta"] = snbt::Tag(snbt::String("Hola guapisimos"));
    snbt::List rewards;
    rewards.push_back(snbt::Tag{"diamante"});
    rewards.push_back(snbt::Tag{"experiencia"});
    rewards.push_back(snbt::Tag{"amor"});
    comp["rewards"] = snbt::Tag{rewards};

    std::ofstream exit("quests/ae3.snbt");
    std::string str;
    str += "{\n";
    for (const auto& [key, value] : comp) {
        str += ("    " + key + ": " + snbt::to_string(value, 1)) + "\n";
    }
    str += "}";
    exit << str << std::endl;
    exit.close();
}

void test5()
{
    std::string input1 = "&aHello &bWorld&r! &kYou wont see me!!!";
    std::cout << "Input 1: " << input1 << "\n";
    std::cout << "JSON 1: " << raw::to_json(input1) << "\n\n";

    std::string input2 = "click &&here&r to &@open";
    std::cout << "Input 2: " << input2 << "\n";
    std::cout << "JSON 2: " << raw::to_json(input2) << "\n\n";

    std::string input3 = "Normal text and &#96615dwith custom color";
    std::cout << "Input 3: " << input3 << "\n";
    std::cout << "JSON 3: " << raw::to_json(input3) << "\n";
}

void test6()
{
    std::string input1 = "Haz clic &b&#eb4236&@url:https://ejemplo.com aquí&r";
    std::cout << "\nInput 1: " << input1 << "\n";
    std::cout << "JSON 1: " << raw::to_json(input1) << "\n\n";

    //doesnt work and idk if im going to try
    std::string input3 = "Ejecutar &@command:\"/give @p minecraft:diamond\" comando";
    std::cout << "Input 3: " << input3 << "\n";
    std::cout << "JSON 3: " << raw::to_json(input3) << "\n\n";

    std::string input4 = "&aInferium&r Scythe - &@url:https://wiki.com/inferium Info &&Haz clic para detalles&r hola soy pepe";
    std::cout << "Input 4: " << input4 << "\n";
    std::cout << "JSON 4: " << raw::to_json(input4) << "\n";
}
*/