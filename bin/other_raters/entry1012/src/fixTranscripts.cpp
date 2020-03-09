/*
 *  Daniil Gentili's submission to the VoIP contest.
 *  Copyright (C) 2019 Daniil Gentili <daniil@daniil.it>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <iterator>
#include <string>
#include <cstring>
#include <cctype>

std::string sentences[39] = {"oak is strong and also gives shade",
                             "cats and dogs each hate the other",
                             "the pipe began to rust while new",
                             "open the crate but don't break the glass",
                             "add the sum to the product of these three",
                             "thieves who rob friends deserve jail",
                             "the ripe taste of cheese improves with age",
                             "act on these orders with great speed",
                             "the hog crawled under the high fence",
                             "move the vat over the hot fire",

                             // Variations
                             "cats and dogs hate the other",
                             "open the crate but don't break glass",

                             // Other sentences
                             "her purse was full of useless trash",
                             "the colt reared and threw the tall rider",
                             "it snowed rained and hailed the same morning",
                             "read verse out loud for pleasure",
                             "hoist the load to your left shoulder",
                             "take the winding path to reach the lake",
                             "note closely the size of the gas tank",
                             "wipe the grease off his dirty face",
                             "mend the coat before you go out",
                             "the wrist was badly strained and hung limp",
                             "the stray cat gave birth to kittens",
                             "the young girl gave no clear response",
                             "the meal was cooked before the bell rang",
                             "what joy there is in living",
                             "a king ruled the state in the early days",
                             "the ship was torn apart on the sharp reef",
                             "sickness kept him home the third week",
                             "the wide road shimmered in the hot sun",
                             "the lazy cow lay in the cool grass",
                             "lift the square stone over the fence",
                             "the rope will bind the seven books at once",
                             "hop over the fence and plunge in",
                             "the friendly gang left the drug store",
                             "mesh wire keeps chicks inside",
                             "the frosty air passed through the coat",
                             "the crooked maze failed to fool the mouse",
                             "adding fast leads to wrong sums"};

// Trim functions taken from https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
// trim from start (in place)
static inline void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
                return !std::isspace(ch);
            }));
}

// trim from end (in place)
static inline void rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
                return !std::isspace(ch);
            })
                .base(),
            s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s)
{
    ltrim(s);
    rtrim(s);
}

// Strips unneeded punctuation from transcripts, lowercases all chars, removes newlines and duplicate spaces
void strip(std::string &input)
{
    std::transform(input.begin(), input.end(), input.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    input.erase(std::remove(input.begin(), input.end(), ','), input.end());
    input.erase(std::remove(input.begin(), input.end(), '.'), input.end());
    std::replace(input.begin(), input.end(), '\n', ' ');

    std::string output = "";
    unique_copy(input.begin(), input.end(), std::back_insert_iterator<std::string>(output),
                [](char a, char b) { return std::isspace(a) && std::isspace(b); });
    input = output;
    trim(input);
}

// Adds a newline before specific sentences
void splitSentences(std::string &input)
{
    for (const std::string &sentence : sentences)
    {
        // Here we have a mix of simple indexes and C++ iterators.
        // Apparently C++'s string APIs were finalized before STL iterators were proposed, so we're stuck with a semi-iterator-friendly API
        size_t pos = 0;
        while ((pos = input.find(sentence, pos)) != std::string::npos)
        {
            if (pos)
            {
                // If we have a space before the current sentence, simply replace it with \n
                if (std::isspace(input[pos - 1]))
                {
                    input.replace(input.begin() + pos - 1, input.begin() + pos, 1, '\n');
                }
                else // Otherwise insert a space between the current sentence and anything that's before it
                {
                    input.insert(input.begin() + pos, '\n');
                    pos++;
                }
            }
            pos += sentence.length();
        }
    }
}

// Substituting newlines with another token
std::string replaceNewline(std::string input, std::string token)
{
    size_t pos = 0;
    while ((pos = input.find('\n', pos)) != std::string::npos)
    {
        if (pos)
        {
            input.replace(pos, 1, token);
        }
        pos += token.length();
    }
    return input;
}
// This script formats manually|automatically generated transcripts, and generates vocabulary files needed for acoustic model generation
int main(int argc, char **argv)
{
    // Whether to generate vocabulary model files
    bool adaptModel = (argc == 2) && !std::strcmp(argv[1], "-m");

    std::ifstream ifs;
    std::ofstream ofs;

    std::string content;

    std::ofstream files;
    std::ofstream transcript;
    std::ofstream vocab;
    std::ofstream vocabClosed;
    if (adaptModel)
    {
        files.open("list.fileids");
        transcript.open("list.transcript");
        vocab.open("list.txt");
        vocabClosed.open("list.closed.txt");
    }
    for (auto &p : std::filesystem::directory_iterator("."))
    {
        // I could've just used C glob, but I wanted to go with a pure C++ approach on this one
        if (p.path().extension() != ".txt")
            continue;
        if (adaptModel)
        {
            if (p.path().filename().string().find("sample") != 0)
                continue; // File name must start with "sample"
        }
        else
        {
            if (p.path().filename().string().find("sample") == std::string::npos)
                continue; // File name must contain "sample"
        }
        ifs.open(p.path());
        content.assign((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
        ifs.close();

        strip(content);

        if (adaptModel)
        {
            std::string file = p.path().stem().string();
            files << file << '\n';
            transcript << "<s> " << content << " </s> (" << file << ")\n";
        }
        splitSentences(content);
        if (adaptModel)
        {
            vocabClosed << content << "\n";
            vocab << "<s> " << replaceNewline(content, " </s>\n<s> ") << " </s>\n";
        }

        ofs.open(p.path());
        ofs << content;
        ofs.close();
    }
}