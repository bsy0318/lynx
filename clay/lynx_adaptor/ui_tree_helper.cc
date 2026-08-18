// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "clay/lynx_adaptor/ui_tree_helper.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "clay/ui/component/page_view.h"
#include "clay/ui/component/view_context.h"
#include "third_party/rapidjson/stringbuffer.h"
#include "third_party/rapidjson/writer.h"

namespace lynx::tasm::ui_tree {
namespace {

using JsonWriter = rapidjson::Writer<rapidjson::StringBuffer>;

void WriteString(JsonWriter& writer, const std::string& value) {
  writer.String(value.c_str(), static_cast<rapidjson::SizeType>(value.size()));
}

void WriteFrame(JsonWriter& writer, const clay::BaseView* view) {
  writer.StartArray();
  writer.Double(view->Left());
  writer.Double(view->Top());
  writer.Double(view->Width());
  writer.Double(view->Height());
  writer.EndArray();
}

void WriteUITreeNode(JsonWriter& writer, clay::BaseView* view);

void WriteUITreeChildren(JsonWriter& writer, clay::BaseView* view) {
  for (auto* child : view->GetChildren()) {
    if (child->IsAnonymousView()) {
      WriteUITreeChildren(writer, child);
    } else {
      WriteUITreeNode(writer, child);
    }
  }
}

void WriteUITreeNode(JsonWriter& writer, clay::BaseView* view) {
  writer.StartObject();
  writer.Key("name");
  WriteString(writer, view->GetName());
  writer.Key("id");
  writer.Int(view->id());
  writer.Key("frame");
  WriteFrame(writer, view);
  writer.Key("children");
  writer.StartArray();
  WriteUITreeChildren(writer, view);
  writer.EndArray();
  writer.EndObject();
}

std::vector<float> ParseFloatArray(const std::string& content) {
  std::string normalized = content;
  std::replace(normalized.begin(), normalized.end(), ',', ' ');
  std::istringstream stream(normalized);
  std::vector<float> values;
  float value = 0;
  while (stream >> value) {
    values.push_back(value);
  }
  stream >> std::ws;
  if (!stream.eof() || values.size() != 4) {
    return {};
  }
  return values;
}

std::string RemoveWhitespace(const std::string& content) {
  std::string normalized;
  normalized.reserve(content.size());
  std::copy_if(content.begin(), content.end(), std::back_inserter(normalized),
               [](unsigned char c) { return !std::isspace(c); });
  return normalized;
}

bool ParseRRGGBBAA(const std::string& content, uint32_t& argb) {
  const std::string normalized = RemoveWhitespace(content);
  if (normalized.size() != 9 || normalized.front() != '#') {
    return false;
  }
  uint32_t rgba = 0;
  const char* begin = normalized.data() + 1;
  const char* end = normalized.data() + normalized.size();
  auto result = std::from_chars(begin, end, rgba, 16);
  if (result.ec != std::errc() || result.ptr != end) {
    return false;
  }
  argb = (rgba >> 8) | ((rgba & 0xff) << 24);
  return true;
}

}  // namespace

std::string GetLynxUITree(clay::ViewContext* view_context) {
  auto* root = view_context ? view_context->GetPageView() : nullptr;
  if (!root || root->id() < 0) {
    return {};
  }
  rapidjson::StringBuffer buffer;
  JsonWriter writer(buffer);
  WriteUITreeNode(writer, root);
  return buffer.GetString();
}

std::string GetUINodeInfo(clay::ViewContext* view_context, int id) {
  auto* view = view_context ? view_context->GetViewById(id) : nullptr;
  if (!view) {
    return {};
  }

  rapidjson::StringBuffer buffer;
  JsonWriter writer(buffer);
  writer.StartObject();
  writer.Key("id");
  writer.Int(view->id());
  writer.Key("editableProps");
  writer.StartObject();
  writer.Key("border");
  writer.StartArray();
  writer.Double(view->BorderTop());
  writer.Double(view->BorderRight());
  writer.Double(view->BorderBottom());
  writer.Double(view->BorderLeft());
  writer.EndArray();
  writer.Key("margin");
  writer.StartArray();
  writer.Double(view->MarginTop());
  writer.Double(view->MarginRight());
  writer.Double(view->MarginBottom());
  writer.Double(view->MarginLeft());
  writer.EndArray();
  writer.Key("frame");
  WriteFrame(writer, view);
  writer.Key("visible");
  writer.Bool(view->Visible());
  writer.EndObject();

  writer.Key("ui");
  writer.StartObject();
  writer.Key("name");
  WriteString(writer, view->GetName());
  writer.Key("readonlyProps");
  writer.StartObject();
  writer.Key("tagName");
  WriteString(writer, view->GetName());
  writer.Key("idSelector");
  WriteString(writer, view->GetIdSelector());
  writer.Key("refIdSelector");
  WriteString(writer, view->GetRefIdSelector());
  writer.Key("attachedToTree");
  writer.Bool(view->attach_to_tree());
  writer.Key("opacity");
  writer.Double(view->Opacity());
  writer.Key("padding");
  writer.StartArray();
  writer.Double(view->PaddingTop());
  writer.Double(view->PaddingRight());
  writer.Double(view->PaddingBottom());
  writer.Double(view->PaddingLeft());
  writer.EndArray();
  writer.EndObject();
  writer.EndObject();

  writer.Key("view");
  writer.StartObject();
  writer.Key("name");
  writer.String("ClayView");
  writer.Key("readonlyProps");
  writer.StartObject();
  writer.Key("frame");
  WriteFrame(writer, view);
  writer.Key("childCount");
  writer.Uint(static_cast<unsigned int>(view->child_count()));
  writer.EndObject();
  writer.EndObject();
  writer.EndObject();
  return buffer.GetString();
}

int SetUIStyle(clay::ViewContext* view_context, int id, const std::string& name,
               const std::string& content) {
  auto* view = view_context ? view_context->GetViewById(id) : nullptr;
  if (!view) {
    return -1;
  }

  if (name == "frame") {
    auto values = ParseFloatArray(content);
    if (values.empty()) {
      return -1;
    }
    view_context->SetBounds(id, values[0], values[1], values[2], values[3]);
  } else if (name == "margin") {
    auto values = ParseFloatArray(content);
    if (values.empty()) {
      return -1;
    }
    const float left = view->Left() - view->MarginLeft() + values[3];
    const float top = view->Top() - view->MarginTop() + values[0];
    const float width = view->Width();
    const float height = view->Height();
    view_context->SetMargins(id, values[3], values[0], values[1], values[2]);
    view_context->SetBounds(id, left, top, width, height);
  } else if (name == "border") {
    auto values = ParseFloatArray(content);
    if (values.empty()) {
      return -1;
    }
    view_context->SetBorderWidth(id, values[3], values[0], values[1],
                                 values[2]);
  } else if (name == "visible") {
    const std::string normalized = RemoveWhitespace(content);
    if (normalized == "true") {
      view->SetVisible(true);
    } else if (normalized == "false") {
      view->SetVisible(false);
    } else {
      return -1;
    }
  } else if (name == "background-color" || name == "border-color") {
    uint32_t color = 0;
    if (!ParseRRGGBBAA(content, color)) {
      return -1;
    }
    if (name == "background-color") {
      view_context->SetBackgroundColor(id, color);
    } else {
      view_context->SetBorderColor(id, color, color, color, color);
    }
  } else {
    return -1;
  }

  view_context->DidUpdateAttributes(id);
  return 0;
}

}  // namespace lynx::tasm::ui_tree
