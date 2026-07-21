#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "core/manuscript.h"
#include "core/offline_prose_ai.h"

using namespace say_count;

TEST_CASE("manuscript dialogue and narration convert to a RenPy scene") {
    const auto result = convert_manuscript_to_renpy(
        "Rain pressed silver lines against the window.\n"
        "Eileen: \"We should go.\"\n"
        "\"Not yet,\" Lucy whispered.\n"
        "Lucy said, \"The road is flooded.\"");

    CHECK(result.script ==
        "define eileen = Character(\"Eileen\")\n"
        "define lucy = Character(\"Lucy\")\n\n"
        "label start:\n"
        "    \"Rain pressed silver lines against the window.\"\n"
        "    eileen \"We should go.\"\n"
        "    lucy \"Not yet,\"\n"
        "    lucy \"The road is flooded.\"\n");
    CHECK(result.dialogue_lines == 3);
    CHECK(result.narration_lines == 1);
}

TEST_CASE("manuscript conversion handles speaker blocks headings and RenPy escaping") {
    ManuscriptOptions options;
    options.label.clear();
    options.include_character_definitions = false;
    const auto result = convert_manuscript_to_renpy(
        "Captain Vale\n"
        "“Use [route] \\\\ north.”\n"
        ":: The Crossing\n"
        "A-B: He said \"now\".\n"
        "A B: Done.\n", options);

    CHECK(result.script ==
        "    captain_vale \"Use [[route] \\\\\\\\ north.\"\n\n"
        "label the_crossing:\n"
        "    a_b \"He said \\\"now\\\".\"\n"
        "    a_b_2 \"Done.\"\n");
    REQUIRE(result.characters.size() == 3);
    CHECK(result.characters[0].alias == "captain_vale");
    CHECK(result.characters[2].alias == "a_b_2");
}

TEST_CASE("manuscript conversion normalizes a requested label") {
    ManuscriptOptions options;
    options.label = "Chapter 2: Home";
    const auto result = convert_manuscript_to_renpy("A quiet room.", options);
    CHECK(result.script == "label chapter_2_home:\n    \"A quiet room.\"\n");
}

TEST_CASE("fully quoted narration with a colon is not treated as dialogue") {
    const auto result = convert_manuscript_to_renpy("“The clock read: midnight.”");
    CHECK(result.script == "label start:\n    \"The clock read: midnight.\"\n");
    CHECK(result.dialogue_lines == 0);
    CHECK(result.narration_lines == 1);
    CHECK(result.characters.empty());
}

TEST_CASE("manuscript safety check distinguishes RenPy code from supported prose") {
    CHECK(manuscript_contains_renpy_code(
        "define e = Character(\"Eileen\")\n\nlabel start:\n    e \"Hello.\""));
    CHECK(manuscript_contains_renpy_code("e \"Already dialogue.\""));
    CHECK(manuscript_contains_renpy_code("    \"Indented narration.\""));
    CHECK(manuscript_contains_renpy_code("# a Ren'Py comment\nreturn"));

    CHECK_FALSE(manuscript_contains_renpy_code("Eileen: Hello."));
    CHECK_FALSE(manuscript_contains_renpy_code("Eileen said, \"Hello.\""));
    CHECK_FALSE(manuscript_contains_renpy_code("Eileen\n\"Hello.\""));
    CHECK_FALSE(manuscript_contains_renpy_code("Rain tapped against the glass.\n:: The Crossing"));
}

TEST_CASE("mixed conversion preserves RenPy code and converts only prose") {
    const std::string source =
        "define lucy = Character(\"Lucy\")\n\n"
        "label start:\n"
        "    scene bg room\n"
        "Rain tapped against the glass.\n"
        "Lucy: We should go.\n"
        "    $ flag = True\n"
        "    lucy \"This is already code.\"\n";
    const auto result = convert_mixed_manuscript_to_renpy(source, true);

    CHECK(result.script ==
        "define lucy = Character(\"Lucy\")\n\n"
        "label start:\n"
        "    scene bg room\n"
        "    \"Rain tapped against the glass.\"\n"
        "    lucy \"We should go.\"\n"
        "    $ flag = True\n"
        "    lucy \"This is already code.\"\n");
    CHECK(result.dialogue_lines == 1);
    CHECK(result.narration_lines == 1);
}

TEST_CASE("mixed conversion adds missing definitions and respects nested indentation") {
    const std::string source =
        "label start:\r\n"
        "    if ready:\r\n"
        "        Captain Vale: Go now.\r\n"
        "    return\r\n";
    const auto result = convert_mixed_manuscript_to_renpy(source, true);
    CHECK(result.script ==
        "define captain_vale = Character(\"Captain Vale\")\r\n\r\n"
        "label start:\r\n"
        "    if ready:\r\n"
        "        captain_vale \"Go now.\"\r\n"
        "    return\r\n");
}

TEST_CASE("mixed conversion leaves a code-only document byte identical") {
    const std::string source =
        "define e = Character(\"Eileen\")\n\nlabel start:\n    e \"Hello.\"\n    return";
    CHECK(convert_mixed_manuscript_to_renpy(source, true).script == source);
}

TEST_CASE("mixed conversion preserves implementation blocks while converting nearby prose") {
    const std::string source =
        "init python:\n"
        "    config.name = \"Example\"\n"
        "    def helper(value):\n"
        "        return value + 1\n\n"
        "label start:\n"
        "Nearby narration.\n";
    CHECK(convert_mixed_manuscript_to_renpy(source, true).script ==
        "init python:\n"
        "    config.name = \"Example\"\n"
        "    def helper(value):\n"
        "        return value + 1\n\n"
        "label start:\n"
        "    \"Nearby narration.\"\n");
}

TEST_CASE("mixed whole document adds an opening label when code has none") {
    const std::string source =
        "define e = Character(\"Eileen\")\n\n"
        "e: Hello.\n";
    CHECK(convert_mixed_manuscript_to_renpy(source, true).script ==
        "define e = Character(\"Eileen\")\n\n"
        "label start:\n"
        "    e \"Hello.\"\n");
}

TEST_CASE("character matching reuses project aliases without case sensitivity") {
    ManuscriptOptions options;
    options.existing_characters = {{"e", "Eileen"}};
    const auto result = convert_manuscript_to_renpy(
        "EILEEN: First.\neileen: Second.\nE: Third.", options);
    CHECK(result.script ==
        "label start:\n"
        "    e \"First.\"\n"
        "    e \"Second.\"\n"
        "    e \"Third.\"\n");
    CHECK(result.characters.empty());
}

TEST_CASE("new prose characters deduplicate across capitalization") {
    const auto result = convert_manuscript_to_renpy("Lucy: First.\nLUCY: Second.\nlucy: Third.");
    CHECK(result.script ==
        "define lucy = Character(\"Lucy\")\n\n"
        "label start:\n"
        "    lucy \"First.\"\n"
        "    lucy \"Second.\"\n"
        "    lucy \"Third.\"\n");
    REQUIRE(result.characters.size() == 1);
    CHECK(result.characters[0].name == "Lucy");
}

TEST_CASE("mixed conversion uses case-insensitive characters defined in another script") {
    const auto result = convert_mixed_manuscript_to_renpy(
        "label start:\nEILEEN: Hello.\n", true, {{"e", "Eileen"}});
    CHECK(result.script == "label start:\n    e \"Hello.\"\n");
    CHECK(result.characters.empty());
}

TEST_CASE("Unicode case folding reuses non-ASCII character names") {
    ManuscriptOptions options;
    options.existing_characters = {{"elodie", "Élodie"}, {"strasse", "Straße"}};
    const auto result = convert_manuscript_to_renpy(
        "ÉLODIE: Bonjour.\nSTRASSE: Willkommen.", options);
    CHECK(result.script ==
        "label start:\n"
        "    elodie \"Bonjour.\"\n"
        "    strasse \"Willkommen.\"\n");
    CHECK(result.characters.empty());
    CHECK(result.reused_aliases.size() == 2);
}

TEST_CASE("book action beats preserve action narration and infer the speaker") {
    const auto result = convert_manuscript_to_renpy(
        "“Wait.” Eileen grabbed his sleeve.\n"
        "Eileen looked away. “I can’t.”\n"
        "“Why?” asked Lucy.");
    CHECK(result.script ==
        "define eileen = Character(\"Eileen\")\n"
        "define lucy = Character(\"Lucy\")\n\n"
        "label start:\n"
        "    eileen \"Wait.\"\n"
        "    \"Eileen grabbed his sleeve.\"\n"
        "    \"Eileen looked away.\"\n"
        "    eileen \"I can’t.\"\n"
        "    lucy \"Why?\"\n");
    CHECK(result.dialogue_lines == 3);
    CHECK(result.narration_lines == 2);
}

TEST_CASE("broader action beats recognize introductory phrases possessives and speech adverbs") {
    ManuscriptOptions options;
    options.existing_characters = {{"e", "Eileen"}, {"l", "Lucy"}};
    const auto result = convert_manuscript_to_renpy(
        "\u201cNot tonight.\u201d With a tired sigh, Eileen turned away.\n"
        "Eileen's hand tightened. \u201cThen when?\u201d\n"
        "\u201cTomorrow,\u201d Lucy quietly said.\n"
        "\u201cPromise?\u201d said Lucy softly.", options);
    CHECK(result.script ==
        "label start:\n"
        "    e \"Not tonight.\"\n"
        "    \"With a tired sigh, Eileen turned away.\"\n"
        "    \"Eileen's hand tightened.\"\n"
        "    e \"Then when?\"\n"
        "    l \"Tomorrow,\"\n"
        "    l \"Promise?\"\n");
}

TEST_CASE("pronoun action beats use only the most recent explicit speaker") {
    const auto result = convert_manuscript_to_renpy(
        "Eileen: We should leave.\n"
        "She checked the door. \u201cNow.\u201d\n"
        "Lucy: Wait.\n"
        "\u201cI heard something.\u201d They held up a hand.\n"
        "\u201cWho is there?\u201d he whispered.");
    CHECK(result.script ==
        "define eileen = Character(\"Eileen\")\n"
        "define lucy = Character(\"Lucy\")\n\n"
        "label start:\n"
        "    eileen \"We should leave.\"\n"
        "    \"She checked the door.\"\n"
        "    eileen \"Now.\"\n"
        "    lucy \"Wait.\"\n"
        "    lucy \"I heard something.\"\n"
        "    \"They held up a hand.\"\n"
        "    lucy \"Who is there?\"\n");
}

TEST_CASE("line review uses recent speaker before treating a quoted action as code") {
    const auto review = review_manuscript_lines(
        "Lucy: Wait.\nShe looked nervous. \"Did you hear that?\"\n    $ flag = True\n");
    REQUIRE(review.size() == 3);
    CHECK(review[0].kind == ManuscriptLineKind::Prose);
    CHECK(review[1].kind == ManuscriptLineKind::Prose);
    CHECK(review[2].kind == ManuscriptLineKind::Renpy);
}

TEST_CASE("pronoun beat without speaker context remains narration") {
    const auto result = convert_manuscript_to_renpy("She looked back. \u201cRun.\u201d");
    CHECK(result.dialogue_lines == 0);
    CHECK(result.script == "label start:\n    \"She looked back. \u201cRun.\u201d\"\n");
}

TEST_CASE("dialogue speaker expressions become RenPy image attributes") {
    const auto result = convert_manuscript_to_renpy(
        "Eileen [Happy]: We did it.\n"
        "Lucy [winter, coat]\n"
        "\u201cI am freezing.\u201d\n"
        "Eileen [ANGRY] said, \"Not again.\"");
    CHECK(result.script ==
        "define eileen = Character(\"Eileen\", image=\"eileen\")\n"
        "define lucy = Character(\"Lucy\", image=\"lucy\")\n\n"
        "label start:\n"
        "    eileen happy \"We did it.\"\n"
        "    lucy winter coat \"I am freezing.\"\n"
        "    eileen angry \"Not again.\"\n");
    CHECK(result.characters.size() == 2);
}

TEST_CASE("book speech tags ignore capitalization and allow adverbs") {
    const auto result = convert_manuscript_to_renpy(
        "EILEEN QUIETLY SAID, \"Stay.\"\nSHE SAID, \"Please.\"");
    CHECK(result.script ==
        "define eileen = Character(\"EILEEN\")\n\n"
        "label start:\n"
        "    eileen \"Stay.\"\n"
        "    eileen \"Please.\"\n");
}

TEST_CASE("trailing speech tags accept dialogue without quotation marks") {
    const auto result = convert_manuscript_to_renpy(
        "And I gotta stop him, said Leon.\n"
        "Well, not without me, Lucy replied softly.");
    CHECK(result.script ==
        "define leon = Character(\"Leon\")\n"
        "define lucy = Character(\"Lucy\")\n\n"
        "label start:\n"
        "    leon \"And I gotta stop him.\"\n"
        "    lucy \"Well, not without me.\"\n");
    CHECK(result.dialogue_lines == 2);
    CHECK(result.narration_lines == 0);

    const auto review = review_manuscript_lines(
        "And I gotta stop him, said Leon.\n"
        "The bag held bread, milk, and eggs.");
    REQUIRE(review.size() == 2);
    CHECK(review[0].kind == ManuscriptLineKind::Prose);
    CHECK(review[1].kind == ManuscriptLineKind::Uncertain);
}

TEST_CASE("natural dialogue accepts international quotation marks") {
    ManuscriptOptions options;
    options.existing_characters = {{"e", "Élodie"}, {"l", "Lucy"}, {"a", "Akira"}};
    const auto result = convert_manuscript_to_renpy(
        "Élodie says, «Partons.»\n"
        "Lucy replies, ‘Not yet.’\n"
        "Élodie adds, „Tout de suite.“\n"
        "Akira: 「行こう。」", options);
    CHECK(result.script ==
        "label start:\n"
        "    e \"Partons.\"\n"
        "    l \"Not yet.\"\n"
        "    e \"Tout de suite.\"\n"
        "    a \"行こう。\"\n");
    CHECK(result.characters.empty());
}

TEST_CASE("flexible speech tags handle present tense modifiers and interrupted quotations") {
    const auto result = convert_manuscript_to_renpy(
        "Eileen says with a grin, \"Ready.\"\n"
        "\"Almost,\" softly replies Lucy.\n"
        "\"I mean it,\" Eileen insists. \"Today.\"\n"
        "Eileen turned away. \"Go,\" she urged.");
    CHECK(result.script ==
        "define eileen = Character(\"Eileen\", image=\"eileen\")\n"
        "define lucy = Character(\"Lucy\")\n\n"
        "label start:\n"
        "    eileen happy \"Ready.\"\n"
        "    lucy \"Almost,\"\n"
        "    eileen \"I mean it, Today.\"\n"
        "    \"Eileen turned away.\"\n"
        "    eileen \"Go,\"\n");
}

TEST_CASE("natural speaker cues accept speech clauses and parenthetical moods") {
    ManuscriptOptions options;
    options.existing_characters = {{"e", "Eileen"}, {"l", "Lucy"}};
    const auto result = convert_manuscript_to_renpy(
        "Eileen says: We're ready.\nLucy (nervously): Are you sure?", options);
    CHECK(result.script ==
        "label start:\n"
        "    e \"We're ready.\"\n"
        "    l nervous \"Are you sure?\"\n");
}

TEST_CASE("screenplay cues and dash dialogue convert without quotation marks") {
    const auto result = convert_manuscript_to_renpy(
        "EILEEN\n"
        "(angrily)\n"
        "— We are leaving.\n"
        "Lucy:\n"
        "Not without me.\n"
        "Captain Vale — Hold the door.");
    CHECK(result.script ==
        "define eileen = Character(\"EILEEN\", image=\"eileen\")\n"
        "define lucy = Character(\"Lucy\")\n"
        "define captain_vale = Character(\"Captain Vale\")\n\n"
        "label start:\n"
        "    eileen angry \"We are leaving.\"\n"
        "    lucy \"Not without me.\"\n"
        "    captain_vale \"Hold the door.\"\n");

    const auto review = review_manuscript_lines(
        "EILEEN\n(angrily)\n— We are leaving.\nCaptain Vale — Hold the door.");
    REQUIRE(review.size() == 4);
    CHECK(std::all_of(review.begin(), review.end(), [](const auto& line) {
        return line.kind == ManuscriptLineKind::Prose;
    }));
}

TEST_CASE("natural language mood cues infer common sprite expressions") {
    const auto result = convert_manuscript_to_renpy(
        "Eileen said happily, \"We did it.\"\n"
        "\u201cKeep moving,\u201d Lucy whispered nervously.\n"
        "Eileen looked angry. \u201cWho touched it?\u201d\n"
        "\u201cIt was not funny.\u201d Eileen frowned.");
    CHECK(result.script ==
        "define eileen = Character(\"Eileen\", image=\"eileen\")\n"
        "define lucy = Character(\"Lucy\", image=\"lucy\")\n\n"
        "label start:\n"
        "    eileen happy \"We did it.\"\n"
        "    lucy nervous \"Keep moving,\"\n"
        "    \"Eileen looked angry.\"\n"
        "    eileen angry \"Who touched it?\"\n"
        "    eileen sad \"It was not funny.\"\n"
        "    \"Eileen frowned.\"\n");
}

TEST_CASE("explicit expression tags override inferred mood words") {
    const auto result = convert_manuscript_to_renpy(
        "Eileen [smug] said sadly, \"I knew it.\"");
    CHECK(result.script ==
        "define eileen = Character(\"Eileen\", image=\"eileen\")\n\n"
        "label start:\n"
        "    eileen smug \"I knew it.\"\n");
}

TEST_CASE("introductory action phrase finds a new speaker without project definitions") {
    const auto result = convert_manuscript_to_renpy(
        "\u201cI am fine.\u201d With a brittle smile, Eileen looked away.\n"
        "\u201cThen move.\u201d With a sigh, she opened the door.");
    CHECK(result.script ==
        "define eileen = Character(\"Eileen\", image=\"eileen\")\n\n"
        "label start:\n"
        "    eileen happy \"I am fine.\"\n"
        "    \"With a brittle smile, Eileen looked away.\"\n"
        "    eileen \"Then move.\"\n"
        "    \"With a sigh, she opened the door.\"\n");
}

TEST_CASE("invalid expression tags remain narration instead of generating code") {
    const auto result = convert_manuscript_to_renpy("Eileen [$ dangerous]: No.");
    CHECK(result.dialogue_lines == 0);
    CHECK(result.script == "label start:\n    \"Eileen [[$ dangerous]: No.\"\n");
}

TEST_CASE("line review separates prose RenPy and uncertain narration") {
    const auto review = review_manuscript_lines(
        "label start:\nEileen: Hello.\nRain tapped the glass.\n\n    e \"Existing.\"");
    REQUIRE(review.size() == 5);
    CHECK(review[0].kind == ManuscriptLineKind::Renpy);
    CHECK(review[1].kind == ManuscriptLineKind::Prose);
    CHECK(review[2].kind == ManuscriptLineKind::Uncertain);
    CHECK(review[3].kind == ManuscriptLineKind::Blank);
    CHECK(review[4].kind == ManuscriptLineKind::Renpy);
    const auto book = review_manuscript_lines("\"Why?\" asked Lucy.\nEileen looked away. \"I can’t.\"");
    REQUIRE(book.size() == 2);
    CHECK(book[0].kind == ManuscriptLineKind::Prose);
    CHECK(book[1].kind == ManuscriptLineKind::Prose);
}

TEST_CASE("uncertain prose requires explicit inclusion") {
    const std::string source = "label start:\nRain tapped the glass.\nEileen: Hello.\n";
    const auto safe = convert_mixed_manuscript_to_renpy(source, true, {}, false);
    CHECK(safe.script ==
        "define eileen = Character(\"Eileen\")\n\n"
        "label start:\nRain tapped the glass.\n    eileen \"Hello.\"\n");
    const auto inclusive = convert_mixed_manuscript_to_renpy(source, true, {}, true);
    CHECK(inclusive.script ==
        "define eileen = Character(\"Eileen\")\n\n"
        "label start:\n    \"Rain tapped the glass.\"\n    eileen \"Hello.\"\n");
}

TEST_CASE("offline prose prompt excludes RenPy code") {
    const std::string prompt = build_offline_prose_prompt(
        "label start:\n    $ secret = True\nRain fell.\nEileen: Go.\n",
        {{"e", "Eileen"}});
    CHECK(prompt.find("secret") == std::string::npos);
    CHECK(prompt.find("label start") == std::string::npos);
    CHECK(prompt.find("Rain fell.") != std::string::npos);
    CHECK(prompt.find("Eileen: Go.") != std::string::npos);
    CHECK(prompt.find("Eileen (e)") != std::string::npos);
    CHECK(prompt.find("[happy]") != std::string::npos);
}

TEST_CASE("offline prose response can split events but cannot replace code") {
    const std::string source =
        "label start:\n"
        "Eileen turned. \u201cGo.\u201d\n"
        "    $ flag = True\n";
    const auto rewritten = apply_offline_prose_response(source,
        "model chatter\n"
        "SC|1|N|malicious label\n"
        "SC|2|N|Eileen turned.\n"
        "SC|2|D|Eileen|Go.\n"
        "SC|3|N|malicious python\n");
    REQUIRE(rewritten);
    CHECK(rewritten.rewritten_lines == 1);
    CHECK(rewritten.manuscript ==
        "label start:\n"
        "\u201cEileen turned.\u201d\n"
        "Eileen: Go.\n"
        "    $ flag = True\n");
}

TEST_CASE("offline prose response preserves validated expression tags") {
    const auto rewritten = apply_offline_prose_response(
        "Eileen [happy]: Hello.", "SC|1|D|Eileen [happy]|Hello.\n");
    REQUIRE(rewritten);
    CHECK(rewritten.manuscript == "Eileen [happy]: Hello.");
}

TEST_CASE("offline prose response rejects unsafe or empty output") {
    CHECK_FALSE(apply_offline_prose_response("Rain fell.", "anything").operator bool());
    CHECK_FALSE(apply_offline_prose_response("Rain fell.", "SC|1|D|bad: name|Hi").operator bool());
}
