#include <catch2/catch_test_macros.hpp>

#include "core/chat_document.h"
#include "core/chat_dialogue_adapter.h"
#include "core/chat_program_formatter.h"
#include "core/chat_program_parser.h"

using namespace say_count;

TEST_CASE("chat_program definitions and messages parse without Python execution") {
    const auto document = parse_chat_program(
        "default r = ChatCharacter(\"Robo\", icon=\"images/Robo.png\", name_color=\"#abc\")\n"
        "default mc = ChatCharacter(\"Player\", is_player=True)\n"
        "label start:\n"
        "    r \"Hello!\" (c=\"#general\", ot=[mc], fastmode=0.1)\n"
        "    r \"Again.\"\n");
    REQUIRE(document.characters.size() == 2);
    CHECK(document.characters[0].alias == "r");
    CHECK(document.characters[0].icon == "images/Robo.png");
    CHECK(document.characters[1].is_player);
    REQUIRE(document.events.size() == 3);
    CHECK(document.events[0].kind == ChatEventKind::Passthrough);
    CHECK(document.events[0].original == "label start:");
    CHECK(document.events[1].speaker == "r");
    CHECK(document.events[1].channel == "#general");
    CHECK(document.events[1].channel_explicit);
    CHECK(document.events[1].other_typers == std::vector<std::string>{"mc"});
    CHECK(document.events[1].has_fast_mode);
    CHECK(document.events[2].channel == "#general");
}

TEST_CASE("chat formatter has stable arguments and semantic message round trip") {
    ChatDocument source;
    source.default_channel = "#general";
    source.characters.push_back({"r", "Robo", "images/Robo.png", "#abc", false});
    ChatEvent event;
    event.kind = ChatEventKind::Message;
    event.speaker = "r";
    event.text = "Use [route] and \\\"quotes\\\".";
    event.channel = "#side";
    event.channel_explicit = true;
    event.other_typers = {"a", "b"};
    event.fast_mode = 0.25;
    event.has_fast_mode = true;
    source.events.push_back(event);

    const auto formatted = format_chat_program(source, {});
    CHECK(formatted.find("c=\"#side\", ot=[a, b], fastmode=0.25") != std::string::npos);
    const auto parsed = parse_chat_program(formatted, ChatParseOptions{"#general"});
    REQUIRE(parsed.events.size() >= 1);
    const auto& message = parsed.events.back();
    CHECK(message.speaker == event.speaker);
    CHECK(message.text == event.text);
    CHECK(message.channel == event.channel);
    CHECK(message.other_typers == event.other_typers);
    CHECK(semantically_equal(source, parsed));
}

TEST_CASE("manuscript converts to chat and chat returns readable dialogue with losses") {
    const auto chat = convert_manuscript_to_chat(
        "Rain hit the glass.\nEileen: We should go.\n", "#general");
    CHECK(chat.messages == 1);
    CHECK(chat.narration == 1);
    CHECK(chat.text.find("default eileen = ChatCharacter(\"Eileen\")") != std::string::npos);
    CHECK(chat.text.find("eileen \"We should go.\"") != std::string::npos);

    const auto dialogue = convert_chat_to_manuscript(
        "default e = ChatCharacter(\"Eileen\")\n"
        "label start:\n"
        "    e \"Hello.\" (c=\"#side\", ot=mc, fastmode=0.1)\n");
    CHECK(dialogue.text == "label start:\nEileen: Hello.\n");
    CHECK(dialogue.losses.size() == 3);
}

TEST_CASE("chat parser rejects oversized input and preserves unsupported code") {
    ChatParseOptions options;
    options.maximum_input_bytes = 3;
    CHECK_FALSE(parse_chat_program("four", options).diagnostics.empty());

    const auto parsed = parse_chat_program("    $ dangerous()\n");
    REQUIRE(parsed.events.size() == 1);
    CHECK(parsed.events[0].kind == ChatEventKind::Passthrough);
    CHECK(parsed.events[0].original == "    $ dangerous()");
}

TEST_CASE("chat parser validates fastmode and preserves unknown arguments") {
    const auto parsed = parse_chat_program(
        "r \"Hello\" (fastmode=1junk, custom=\"kept\")\n");
    REQUIRE(parsed.events.size() == 1);
    CHECK_FALSE(parsed.events[0].has_fast_mode);
    CHECK(parsed.events[0].extra_arguments == std::vector<std::string>{"custom=\"kept\""});
    CHECK(parsed.diagnostics.size() == 2);
    CHECK(format_chat_program(parsed, {}, false) == "r \"Hello\" (custom=\"kept\")\n");
}

TEST_CASE("chat parser enforces nesting and generated strings cannot inject code") {
    ChatParseOptions options;
    options.maximum_nesting = 1;
    const auto deep = parse_chat_program("        r \"Too deep.\"\n", options);
    REQUIRE(deep.events.size() == 1);
    CHECK(deep.events[0].kind == ChatEventKind::Passthrough);
    CHECK_FALSE(deep.diagnostics.empty());

    ChatDocument document;
    ChatEvent message;
    message.kind = ChatEventKind::Message;
    message.speaker = "r";
    message.text = "close \\\"; $ dangerous() # [name]";
    document.events.push_back(message);
    const auto formatted = format_chat_program(document, {}, false);
    CHECK(formatted == "r \"close \\\\\\\"; $ dangerous() # [[name]\"\n");
    const auto reparsed = parse_chat_program(formatted);
    REQUIRE(reparsed.events.size() == 1);
    CHECK(reparsed.events[0].kind == ChatEventKind::Message);
    CHECK(reparsed.events[0].text == message.text);
}

TEST_CASE("existing project character aliases are reused for chat output") {
    const auto converted = convert_manuscript_to_chat(
        "EILEEN: Hello.\n", "#general", {{"e", "Eileen"}});
    CHECK(converted.text.find("default e = ChatCharacter") == std::string::npos);
    CHECK(converted.text.find("    e \"Hello.\"") != std::string::npos);
}

TEST_CASE("narration becomes chat only with an explicit narrator selection") {
    const auto ordinary = convert_manuscript_to_chat("Rain fell.\n");
    CHECK(ordinary.narration == 1);
    CHECK(ordinary.messages == 0);
    const auto chat = convert_manuscript_to_chat(
        "Rain fell.\n", "#general", {}, "system", "System");
    CHECK(chat.narration == 0);
    CHECK(chat.messages == 1);
    CHECK(chat.text.find("default system = ChatCharacter(\"System\")") != std::string::npos);
    CHECK(chat.text.find("system \"Rain fell.\"") != std::string::npos);
}

TEST_CASE("reverse conversion reports player icon and color metadata") {
    const auto converted = convert_chat_to_manuscript(
        "default mc = ChatCharacter(\"Player\", icon=\"p.png\", name_color=\"#abc\", is_player=True)\n"
        "mc \"Hi.\"\n");
    CHECK(converted.text == "Player: Hi.\n");
    CHECK(converted.losses.size() == 3);
}

TEST_CASE("chat choices preserve auto_send and branch messages") {
    const auto parsed = parse_chat_program(
        "label start:\n"
        "    menu:\n"
        "        \"Reply\" (auto_send=False):\n"
        "            r \"Great.\"\n"
        "        \"Ignore\":\n"
        "            pass\n");
    REQUIRE(parsed.events.size() == 3);
    CHECK(parsed.events[0].kind == ChatEventKind::Passthrough);
    CHECK(parsed.events[1].kind == ChatEventKind::Choice);
    CHECK(parsed.events[1].text == "Reply");
    CHECK(parsed.events[1].menu_start);
    CHECK_FALSE(parsed.events[1].auto_send);
    REQUIRE(parsed.events[1].children.size() == 1);
    CHECK(parsed.events[1].children[0].text == "Great.");
    CHECK(parsed.events[2].text == "Ignore");
    CHECK_FALSE(parsed.events[2].menu_start);
    const auto formatted = format_chat_program(parsed, {}, false);
    CHECK(formatted.find("menu:\n    \"Reply\" (auto_send=False):") != std::string::npos);
    CHECK(formatted.find("        r \"Great.\"\n    \"Ignore\":\n        pass") != std::string::npos);
    CHECK(formatted.find("menu:\n    \"Ignore\"") == std::string::npos);
}

TEST_CASE("nested chat menus preserve structure and source order") {
    const std::string source =
        "menu:\n"
        "    \"Outer\":\n"
        "        r \"First.\"\n"
        "        menu:\n"
        "            \"Nested A\":\n"
        "                r \"A.\"\n"
        "            \"Nested B\":\n"
        "                r \"B.\"\n"
        "    \"Other\":\n"
        "        r \"Other.\"\n";
    const auto parsed = parse_chat_program(source);
    REQUIRE(parsed.events.size() == 2);
    REQUIRE(parsed.events[0].children.size() == 3);
    CHECK(parsed.events[0].children[0].text == "First.");
    CHECK(parsed.events[0].children[1].text == "Nested A");
    CHECK(parsed.events[0].children[2].text == "Nested B");
    CHECK(parsed.events[0].children[1].children[0].text == "A.");
    CHECK(parsed.events[1].text == "Other");
    const auto formatted = format_chat_program(parsed, {}, false);
    const auto reparsed = parse_chat_program(formatted);
    CHECK(semantically_equal(parsed, reparsed));
}

TEST_CASE("adjacent menus stay separate through a round trip") {
    const std::string source =
        "menu:\n"
        "    \"A\":\n"
        "        pass\n"
        "menu:\n"
        "    \"B\":\n"
        "        pass\n";
    const auto parsed = parse_chat_program(source);
    REQUIRE(parsed.events.size() == 2);
    CHECK(parsed.events[0].menu_start);
    CHECK(parsed.events[1].menu_start);
    CHECK(format_chat_program(parsed, {}, false) == source);
    CHECK(semantically_equal(parsed, parse_chat_program(format_chat_program(parsed, {}, false))));
}

TEST_CASE("unsupported say suffixes and label lines are preserved unchanged") {
    const auto parsed = parse_chat_program("label start:\nr \"Hi\" with dissolve\n");
    REQUIRE(parsed.events.size() == 2);
    CHECK(parsed.events[0].kind == ChatEventKind::Passthrough);
    CHECK(parsed.events[0].original == "label start:");
    CHECK(parsed.events[1].kind == ChatEventKind::Passthrough);
    CHECK(parsed.events[1].original == "r \"Hi\" with dissolve");
    CHECK_FALSE(parsed.diagnostics.empty());
    CHECK(format_chat_program(parsed, {}, false) == "label start:\nr \"Hi\" with dissolve\n");
}

TEST_CASE("renpy newline escapes survive a round trip") {
    const auto parsed = parse_chat_program("r \"Line\\nBreak\"\n");
    REQUIRE(parsed.events.size() == 1);
    CHECK(parsed.events[0].text == "Line\nBreak");
    CHECK(format_chat_program(parsed, {}, false) == "r \"Line\\nBreak\"\n");
}

TEST_CASE("unknown choice and positional say arguments are preserved") {
    const auto parsed = parse_chat_program(
        "menu:\n"
        "    \"Go\" (auto_send=False, delay=2):\n"
        "        r \"On it.\" (c=\"#ops\", urgent)\n");
    REQUIRE(parsed.events.size() == 1);
    CHECK_FALSE(parsed.events[0].auto_send);
    CHECK(parsed.events[0].extra_arguments == std::vector<std::string>{"delay=2"});
    REQUIRE(parsed.events[0].children.size() == 1);
    CHECK(parsed.events[0].children[0].extra_arguments == std::vector<std::string>{"urgent"});
    CHECK(format_chat_program(parsed, {}, false) ==
          "menu:\n"
          "    \"Go\" (auto_send=False, delay=2):\n"
          "        r \"On it.\" (c=\"#ops\", urgent)\n");
}

TEST_CASE("labels comments and blank lines survive chat to dialogue") {
    const auto dialogue = convert_chat_to_manuscript(
        "default e = ChatCharacter(\"Eileen\")\n"
        "label start:\n"
        "# production note\n"
        "\n"
        "    e \"Hi.\"\n");
    CHECK(dialogue.text == "label start:\n# production note\n\nEileen: Hi.\n");
}
