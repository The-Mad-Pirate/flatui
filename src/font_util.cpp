// Copyright 2016 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "precompiled.h"

#include <cctype>
#include <sstream>

#include "../../gumbo-parser/src/gumbo.h"  // TODO: Install properly
#include "font_manager.h"
#include "font_util.h"
#include "fplbase/fpl_common.h"

namespace flatui {

// Replace whitespace with a single space.
// This emulates how HTML processes raw text fields.
// Appends trimmed text to `out`.
// Returns reference to `out`.
std::string &TrimHtmlWhitespace(const char *text, bool trim_leading_whitespace,
                                std::string *out) {
  const char *t = text;

  if (trim_leading_whitespace) {
    while (std::isspace(*t)) ++t;
  }

  for (;;) {
    // Output non-whitespace.
    while (!std::isspace(*t) && *t != '\0') out->push_back(*t++);

    // Break out if at end of input. Note that we do *not* trim all
    // trailing whitespace.
    if (*t == '\0') break;

    // Skip whitespace.
    while (std::isspace(*t)) ++t;

    // Insert single space to compensate for trimmed whitespace.
    out->push_back(' ');
  }
  return *out;
}

// Remove trailing whitespace. Then add a `prefix` if there is any preceeding
// text.
static std::string &StartHtmlLine(const char* prefix, std::string *out) {
  // Trim trailing whitespace.
  while (std::isspace(out->back())) out->pop_back();

  // Append the new lines in `prefix`.
  if (!out->empty()) {
    out->append(prefix);
  }
  return *out;
}

// The very first text output we want to trim the leading whitespace.
// We should also trim if the previous text ends in whitespace.
static bool ShouldTrimLeadingWhitespace(const std::vector<HtmlSection>& s) {
  if (s.size() <= 1) return true;

  const std::string& prev_text = s.back().text.empty() ?
                                 s[s.size() - 2].text : s.back().text;
  return std::isspace(prev_text.back());
}

static void GumboTreeToHtmlSections(const GumboNode *node,
                                    std::vector<HtmlSection> *s) {
  switch (node->type) {
    // Process non-text elements, possibly recursing into child nodes.
    case GUMBO_NODE_ELEMENT: {
      const GumboElement &element = node->v.element;

      // Tree prefix processing.
      switch (element.tag) {
        case GUMBO_TAG_A:
          // Start a new section for the anchor.
          if (!s->back().text.empty()) {
            s->push_back(HtmlSection());
          }
          break;

        case GUMBO_TAG_P:
        case GUMBO_TAG_H1:  // fallthrough
        case GUMBO_TAG_H2:  // fallthrough
        case GUMBO_TAG_H3:  // fallthrough
        case GUMBO_TAG_H4:  // fallthrough
        case GUMBO_TAG_H5:  // fallthrough
        case GUMBO_TAG_H6:  // fallthrough
          StartHtmlLine("\n\n", &s->back().text);
          break;

        default:
          break;
      }
      const size_t node_section = s->size() - 1;

      // Tree children processing via recursion.
      for (unsigned int i = 0; i < element.children.length; ++i) {
        const GumboNode *child =
            static_cast<GumboNode *>(element.children.data[i]);
        GumboTreeToHtmlSections(child, s);
      }

      // Tree postfix processing.
      switch (element.tag) {
        case GUMBO_TAG_A: {
          // Record the link address.
          GumboAttribute *href =
              gumbo_get_attribute(&element.attributes, "href");
          if (href != nullptr) {
            (*s)[node_section].link = href->value;
          }

          // Start a new section for the non-anchor.
          s->push_back(HtmlSection());
          break;
        }

        case GUMBO_TAG_HR:
        case GUMBO_TAG_P:   // fallthrough
          s->back().text.append("\n\n");
          break;

        case GUMBO_TAG_H1:  // fallthrough
        case GUMBO_TAG_H2:  // fallthrough
        case GUMBO_TAG_H3:  // fallthrough
        case GUMBO_TAG_H4:  // fallthrough
        case GUMBO_TAG_H5:  // fallthrough
        case GUMBO_TAG_H6:  // fallthrough
        case GUMBO_TAG_BR:
          s->back().text.append("\n");
          break;

        default:
          break;
      }
      break;
    }

    // Append text without excessive whitespaces.
    case GUMBO_NODE_TEXT:
      TrimHtmlWhitespace(node->v.text.text, ShouldTrimLeadingWhitespace(*s),
                         &s->back().text);
      break;

    // Ignore other node types.
    default:
      break;
  }
}

std::vector<HtmlSection> ParseHtml(const char *html) {
  // Ensure there is an HtmlSection that can be appended to.
  std::vector<HtmlSection> s(1);

  // Parse html into tree, with Gumbo, then process the tree.
  GumboOutput *gumbo = gumbo_parse(html);
  GumboTreeToHtmlSections(gumbo->root, &s);
  gumbo_destroy_output(&kGumboDefaultOptions, gumbo);

  // Prune empty last section.
  if (s.back().text.empty()) {
    s.pop_back();
  }
  return s;
}

}  // namespace flatui
