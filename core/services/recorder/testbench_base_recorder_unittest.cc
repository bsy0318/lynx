// Copyright 2023 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <memory>

#define private public
#include "core/services/recorder/testbench_base_recorder.h"
#undef private
#include <mutex>

#include "core/services/recorder/recorder_controller.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace lynx {
namespace tasm {
namespace recorder {

void wait(fml::Thread& thread) {
  std::condition_variable condition;
  std::mutex local_mutex;
  std::unique_lock<std::mutex> lock(local_mutex);
  thread.GetTaskRunner()->PostTask([&condition, &local_mutex]() {
    std::unique_lock<std::mutex> lock(local_mutex);
    condition.notify_one();
  });
  condition.wait(lock);
}

TEST(TestBenchBaseRecorder, RecordScripts) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  int64_t record_id = 1;
  ark.Clear();
  ark.is_recording_ = false;

  std::string url = "url";
  std::string content = "content";
  ark.RecordScripts(url.c_str(), content.c_str(), record_id);
  wait(ark.thread_);

  ASSERT_EQ(ark.script_cache_.count(record_id), 1);
  ASSERT_EQ(ark.script_cache_[record_id].count(url), 1);
  EXPECT_STREQ(ark.script_cache_[record_id][url].c_str(),
               "eJxLzs8rSc0rAQALywL8");

  // Scripts loaded before recording starts are copied into the new record.
  ark.StartRecord();
  rapidjson::Value& scripts_table = ark.GetRecordedFile(record_id)[kScripts];
  ASSERT_TRUE(scripts_table.IsObject());
  EXPECT_EQ(scripts_table.MemberCount(), 1);
  ASSERT_TRUE(scripts_table.HasMember(url));
  EXPECT_STREQ((scripts_table[url]).GetString(), "eJxLzs8rSc0rAQALywL8");

  ark.is_recording_ = false;
  ark.Clear();
}

TEST(TestBenchBaseRecorder, RecordScriptsPreservesBinaryData) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t record_id = 2;
  ark.Clear();
  ark.is_recording_ = false;

  const std::string content("abc\0def", 7);
  ark.RecordScripts("binary-script", content, record_id);
  wait(ark.thread_);

  ASSERT_EQ(ark.script_cache_.count(record_id), 1);
  EXPECT_EQ(ark.script_cache_[record_id]["binary-script"],
            "eJxLTEpmSElNAwAJRQJW");
  ark.Clear();
}

TEST(TestBenchBaseRecorder, RejectsExternalScriptBeforeRecordingStarts) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t record_id = 3;
  ark.Clear();
  ark.is_recording_ = false;

  ark.RecordExternalScript("app-service.js", "external source");
  wait(ark.thread_);

  ASSERT_EQ(ark.external_script_cache_.count("app-service.js"), 0);
  ark.StartRecord();
  rapidjson::Value& scripts = ark.GetRecordedFile(record_id)[kScripts];
  ASSERT_TRUE(scripts.IsObject());
  ASSERT_FALSE(scripts.HasMember("app-service.js"));

  ark.is_recording_ = false;
  ark.Clear();
}

TEST(TestBenchBaseRecorder, ExternalScriptIsClearedAfterRecordingSession) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t first_record_id = 31;
  constexpr int64_t second_record_id = 32;
  ark.Clear();
  ark.SetRecorderPath("/tmp");

  ark.StartRecord();
  ark.RecordExternalScript("lynx-main-thread.js", "main thread source");
  wait(ark.thread_);

  ASSERT_TRUE(ark.GetRecordedFile(first_record_id)[kScripts].HasMember(
      "lynx-main-thread.js"));

  std::atomic_bool completed{false};
  ark.EndRecord(base::MoveOnlyClosure<void, std::vector<std::string>&,
                                      std::vector<int64_t>&>(
      [&completed](std::vector<std::string>&, std::vector<int64_t>&) {
        completed = true;
      }));
  wait(ark.thread_);

  ASSERT_TRUE(completed);
  ASSERT_EQ(ark.external_script_cache_.count("lynx-main-thread.js"), 0);

  ark.StartRecord();
  rapidjson::Value& second_scripts =
      ark.GetRecordedFile(second_record_id)[kScripts];
  ASSERT_FALSE(second_scripts.HasMember("lynx-main-thread.js"));

  ark.is_recording_ = false;
  ark.Clear();
  std::remove("/tmp/31.json");
}

TEST(TestBenchBaseRecorder, ExternalScriptBridgePreservesBinaryData) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t record_id = 4;
  ark.Clear();

  const char source[] = {'a', 'b', 'c', '\0', 'd', 'e', 'f'};
  ark.StartRecord();
  LynxTestBenchRecordExternalScriptWithSize("", source, sizeof(source));
  LynxTestBenchRecordExternalScriptWithSize("empty.js", source, 0);
  LynxTestBenchRecordExternalScriptWithSize("template.js", source,
                                            sizeof(source));
  wait(ark.thread_);

  ASSERT_EQ(ark.external_script_cache_.size(), 1);
  ASSERT_EQ(ark.external_script_cache_.count("template.js"), 1);
  EXPECT_EQ(ark.external_script_cache_["template.js"], "eJxLTEpmSElNAwAJRQJW");

  rapidjson::Value& scripts = ark.GetRecordedFile(record_id)[kScripts];
  ASSERT_TRUE(scripts.HasMember("template.js"));
  EXPECT_STREQ(scripts["template.js"].GetString(), "eJxLTEpmSElNAwAJRQJW");
  ark.Clear();
}

TEST(TestBenchBaseRecorder, RecordPreloadScriptsBeforeRecordingStarts) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t record_id = 3;
  ark.Clear();
  ark.is_recording_ = false;

  ark.RecordPreloadScript("example-runtime.js", "preload", record_id);
  wait(ark.thread_);

  ark.StartRecord();
  rapidjson::Value& preload_scripts =
      ark.GetRecordedFile(record_id)[kPreloadScripts];
  ASSERT_TRUE(preload_scripts.IsObject());
  ASSERT_TRUE(preload_scripts.HasMember("example-runtime.js"));
  EXPECT_STREQ(preload_scripts["example-runtime.js"].GetString(),
               "eJwrKErNyU9MAQAL3wLo");
  rapidjson::Value& preload_script_paths =
      ark.GetRecordedFile(record_id)[kPreloadScriptPaths];
  ASSERT_TRUE(preload_script_paths.IsArray());
  ASSERT_EQ(preload_script_paths.Size(), 1);
  EXPECT_STREQ(preload_script_paths[0].GetString(), "example-runtime.js");
  ark.is_recording_ = false;
  ark.Clear();
}

TEST(TestBenchBaseRecorder, IgnoresPreloadScriptsWithoutRecordID) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  ark.Clear();

  ark.RecordPreloadScript("example-runtime.js", "preload", 0);
  wait(ark.thread_);

  EXPECT_EQ(ark.preload_script_cache_.count(0), 0);
  EXPECT_EQ(ark.preload_script_paths_cache_.count(0), 0);
  EXPECT_EQ(ark.lynx_view_table_.count(0), 0);
  ark.Clear();
}

void CheckLynxViewTable(TestBenchBaseRecorder& ark, int64_t record_id) {
  rapidjson::Value& recorded_file = ark.lynx_view_table_[record_id];

  ASSERT_TRUE(recorded_file.IsObject());
  EXPECT_EQ(recorded_file.MemberCount(), 9);

  rapidjson::Value& action_list = recorded_file[kActionList];
  ASSERT_TRUE(action_list.IsArray());
  EXPECT_EQ(action_list.Size(), 0);

  rapidjson::Value& invoked_method_data = recorded_file[kInvokedMethodData];
  ASSERT_TRUE(invoked_method_data.IsArray());
  EXPECT_EQ(invoked_method_data.Size(), 0);

  rapidjson::Value& callback = recorded_file[kCallback];
  ASSERT_TRUE(callback.IsObject());
  EXPECT_EQ(callback.MemberCount(), 0);

  rapidjson::Value& component_list_value = recorded_file[kComponentList];
  ASSERT_TRUE(component_list_value.IsArray());
  EXPECT_EQ(component_list_value.Size(), 0);

  rapidjson::Value& debugInfo = recorded_file[kDebugInfo];
  ASSERT_TRUE(debugInfo.IsArray());
  EXPECT_EQ(debugInfo.Size(), 0);

  rapidjson::Value& shared_data = recorded_file[kSharedData];
  ASSERT_TRUE(shared_data.IsObject());
  EXPECT_EQ(shared_data.MemberCount(), 0);

  rapidjson::Value& scripts = recorded_file[kScripts];
  ASSERT_TRUE(scripts.IsObject());
  EXPECT_EQ(scripts.MemberCount(), 0);

  rapidjson::Value& preload_scripts = recorded_file[kPreloadScripts];
  ASSERT_TRUE(preload_scripts.IsObject());
  EXPECT_EQ(preload_scripts.MemberCount(), 0);

  rapidjson::Value& preload_script_paths = recorded_file[kPreloadScriptPaths];
  ASSERT_TRUE(preload_script_paths.IsArray());
  EXPECT_EQ(preload_script_paths.Size(), 0);
}

TEST(TestBenchBaseRecorder, Clear) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  int64_t record_id = 1;

  ark.StartRecord();
  ark.GetRecordedFile(record_id);
  ark.SetScreenSize(record_id, 123, 456);
  wait(ark.thread_);
  ark.AddLynxViewSessionID(record_id, 42);
  ark.url_map_[record_id] = "url";

  EXPECT_EQ(ark.lynx_view_table_.size(), 1);
  EXPECT_EQ(ark.replay_config_map_.size(), 1);
  EXPECT_EQ(ark.url_map_.size(), 1);
  EXPECT_EQ(ark.session_ids_.size(), 1);

  ark.Clear();

  EXPECT_EQ(ark.lynx_view_table_.size(), 0);
  EXPECT_EQ(ark.replay_config_map_.size(), 0);
  EXPECT_EQ(ark.url_map_.size(), 0);
  EXPECT_EQ(ark.session_ids_.size(), 0);
  EXPECT_FALSE(ark.is_recording_);
}

TEST(TestBenchBaseRecorder, RemoveRecord) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  int64_t record_id = 1;

  ark.Clear();
  ark.is_recording_ = false;
  ark.RecordScripts("script.js", "script", record_id);
  ark.RecordPreloadScript("preload.js", "preload", record_id);
  ark.GetRecordedFile(record_id);
  ark.SetScreenSize(record_id, 123, 456);
  wait(ark.thread_);
  ark.AddLynxViewSessionID(record_id, 42);

  EXPECT_EQ(ark.lynx_view_table_.count(record_id), 1);
  EXPECT_EQ(ark.replay_config_map_.count(record_id), 1);
  EXPECT_EQ(ark.session_ids_.count(record_id), 1);

  ark.RemoveRecord(record_id);
  wait(ark.thread_);

  EXPECT_EQ(ark.lynx_view_table_.count(record_id), 0);
  EXPECT_EQ(ark.replay_config_map_.count(record_id), 0);
  EXPECT_EQ(ark.url_map_.count(record_id), 0);
  EXPECT_EQ(ark.session_ids_.count(record_id), 0);
  EXPECT_EQ(ark.script_cache_.count(record_id), 0);
  EXPECT_EQ(ark.preload_script_cache_.count(record_id), 0);
  EXPECT_EQ(ark.preload_script_paths_cache_.count(record_id), 0);

  ark.Clear();
}

TEST(TestBenchBaseRecorder, CreateRecordedFile) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  EXPECT_EQ(ark.lynx_view_table_.size(), 0);
  int64_t record_id = 1;
  ark.CreateRecordedFile(record_id);
  EXPECT_EQ(ark.lynx_view_table_.size(), 1);
  CheckLynxViewTable(ark, record_id);
  ark.Clear();
}

TEST(TestBenchBaseRecorder, GetRecordedFile) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  ark.GetRecordedFile(1);
  CheckLynxViewTable(ark, 1);
  EXPECT_EQ(ark.lynx_view_table_.size(), 1);
  ark.GetRecordedFile(2);
  CheckLynxViewTable(ark, 2);
  EXPECT_EQ(ark.lynx_view_table_.size(), 2);
  ark.Clear();
}

TEST(TestBenchBaseRecorder, GetRecordedFileField) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  int64_t record_id = 1;
  rapidjson::Value& value = ark.GetRecordedFileField(record_id, kActionList);
  ASSERT_TRUE(value.IsArray());
  EXPECT_EQ(value.Size(), 0);

  value = ark.GetRecordedFileField(record_id, kInvokedMethodData);
  ASSERT_TRUE(value.IsArray());
  EXPECT_EQ(value.Size(), 0);

  value = ark.GetRecordedFileField(record_id, kCallback);
  ASSERT_TRUE(value.IsObject());
  EXPECT_EQ(value.MemberCount(), 0);

  value = ark.GetRecordedFileField(record_id, kComponentList);
  ASSERT_TRUE(value.IsArray());
  EXPECT_EQ(value.Size(), 0);
  ark.Clear();
}

TEST(TestBenchBaseRecorder, RecordInvokedMethodCreatesFileForNewRecord) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t record_id = 90210;
  ark.Clear();
  ark.StartRecord();

  rapidjson::Value params(rapidjson::kObjectType);
  params.AddMember(rapidjson::StringRef(kParamArgc), 0, ark.GetAllocator());
  params.AddMember(rapidjson::StringRef(kParamReturnValue), "undefined",
                   ark.GetAllocator());
  ark.RecordInvokedMethodData("ExampleBridge", "invoke", params, record_id);
  wait(ark.thread_);

  ASSERT_EQ(ark.lynx_view_table_.count(record_id), 1);
  rapidjson::Value& invoked_method_data =
      ark.lynx_view_table_[record_id][kInvokedMethodData];
  ASSERT_TRUE(invoked_method_data.IsArray());
  ASSERT_EQ(invoked_method_data.Size(), 1);
  EXPECT_STREQ(invoked_method_data[0][kModuleName].GetString(),
               "ExampleBridge");
  EXPECT_STREQ(invoked_method_data[0][kMethodName].GetString(), "invoke");

  ark.Clear();
}

TEST(TestBenchBaseRecorder, ExternalRecordBridgeRejectsDataBeforeRecording) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t record_id = 90213;
  ark.Clear();

  EXPECT_FALSE(LynxTestBenchRecorderRecordInvokedMethod(
      record_id, "ExampleBridge", "invoke",
      R"({"argc":1,"args":["bootstrap"],"returnValue":"ready"})"));
  EXPECT_FALSE(LynxTestBenchRecorderRecordCallbackWithGeneration(
      record_id, "ExampleBridge", "invoke", 43, R"({"returnValue":"done"})",
      1));
  wait(ark.thread_);

  EXPECT_EQ(ark.lynx_view_table_.count(record_id), 0);
}

TEST(TestBenchBaseRecorder, ExternalRecordBridgeUsesStandardRecordShape) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t record_id = 90211;
  ark.Clear();
  EXPECT_FALSE(LynxTestBenchRecorderIsRecording());

  ark.StartRecord();
  const char* params = R"({"argc":0,"args":[],"returnValue":"undefined"})";
  uint64_t recording_generation =
      LynxTestBenchRecorderRecordInvokedMethodWithGeneration(
          record_id, "ExampleBridge", "invoke", params);
  EXPECT_NE(recording_generation, 0u);
  EXPECT_FALSE(LynxTestBenchRecorderRecordInvokedMethod(0, "ExampleBridge",
                                                        "invoke", params));
  EXPECT_FALSE(LynxTestBenchRecorderRecordInvokedMethod(record_id, "", "invoke",
                                                        params));
  EXPECT_FALSE(LynxTestBenchRecorderRecordCallbackWithGeneration(
      0, "ExampleBridge", "invoke", 1, params, recording_generation));
  EXPECT_FALSE(LynxTestBenchRecorderRecordInvokedMethod(
      record_id, "ExampleBridge", "invoke", "invalid-json"));
  wait(ark.thread_);

  ASSERT_EQ(ark.lynx_view_table_.count(record_id), 1);
  rapidjson::Value& invoked_method_data =
      ark.lynx_view_table_[record_id][kInvokedMethodData];
  ASSERT_EQ(invoked_method_data.Size(), 1);
  EXPECT_STREQ(invoked_method_data[0][kModuleName].GetString(),
               "ExampleBridge");
  EXPECT_STREQ(invoked_method_data[0][kMethodName].GetString(), "invoke");

  ark.Clear();
}

TEST(TestBenchBaseRecorder, RejectsCallbackFromPreviousRecordingGeneration) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t record_id = 90217;
  ark.Clear();
  ark.SetRecorderPath("/tmp");
  ark.StartRecord();

  uint64_t first_generation =
      LynxTestBenchRecorderRecordInvokedMethodWithGeneration(
          record_id, "ExampleBridge", "invoke",
          R"({"argc":1,"args":["getSystemInfo"],"returnValue":"undefined"})");
  ASSERT_NE(first_generation, 0u);
  wait(ark.thread_);

  ark.EndRecord(base::MoveOnlyClosure<void, std::vector<std::string>&,
                                      std::vector<int64_t>&>(
      [](std::vector<std::string>&, std::vector<int64_t>&) {}));
  wait(ark.thread_);
  ark.StartRecord();
  ASSERT_GT(LynxTestBenchRecorderRecordingGeneration(), first_generation);

  EXPECT_FALSE(LynxTestBenchRecorderRecordCallbackWithGeneration(
      record_id, "ExampleBridge", "invoke", 7,
      R"({"returnValue":{"errMsg":"getSystemInfo:ok"}})", first_generation));
  wait(ark.thread_);
  EXPECT_EQ(ark.lynx_view_table_.count(record_id), 0);

  ark.Clear();
  std::remove("/tmp/90217.json");
}

TEST(TestBenchBaseRecorder, AcceptsCallbackFromCurrentRecordingGeneration) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t record_id = 90218;
  ark.Clear();
  ark.StartRecord();

  uint64_t recording_generation =
      LynxTestBenchRecorderRecordInvokedMethodWithGeneration(
          record_id, "ExampleBridge", "invoke",
          R"({"argc":1,"args":["getSystemInfo"],"returnValue":"undefined","callback":["7"]})");
  ASSERT_NE(recording_generation, 0u);
  EXPECT_TRUE(LynxTestBenchRecorderRecordCallbackWithGeneration(
      record_id, "ExampleBridge", "invoke", 7,
      R"({"returnValue":{"errMsg":"getSystemInfo:ok"}})",
      recording_generation));
  wait(ark.thread_);

  rapidjson::Value& callback = ark.lynx_view_table_[record_id][kCallback];
  ASSERT_TRUE(callback.HasMember("7"));
  EXPECT_STREQ(callback["7"][kParams]["returnValue"]["errMsg"].GetString(),
               "getSystemInfo:ok");

  ark.Clear();
}

TEST(TestBenchBaseRecorder, RepeatedStartKeepsCurrentRecordingGeneration) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  ark.Clear();
  ark.StartRecord();
  uint64_t recording_generation = LynxTestBenchRecorderRecordingGeneration();

  ark.StartRecord();

  EXPECT_EQ(LynxTestBenchRecorderRecordingGeneration(), recording_generation);
  ark.Clear();
}

TEST(TestBenchBaseRecorder, ExternalRecordBridgeInitializesReplayConfig) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t record_id = 90216;
  ark.Clear();

  LynxTestBenchRecorderInitConfig("/tmp", -1, 390, 844, record_id);
  wait(ark.thread_);

  EXPECT_STREQ(ark.file_path_.c_str(), "/tmp/");
  ASSERT_TRUE(ark.replay_config_map_[record_id].IsObject());
  EXPECT_EQ(ark.session_ids_[record_id], -1);
  EXPECT_FLOAT_EQ(ark.replay_config_map_[record_id]["screenWidth"].GetFloat(),
                  390);
  EXPECT_FLOAT_EQ(ark.replay_config_map_[record_id]["screenHeight"].GetFloat(),
                  844);

  ark.Clear();
}

TEST(TestBenchBaseRecorder, InitConfigCanFollowSynchronousStartRecord) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t record_id = 90217;
  ark.Clear();

  ark.StartRecord();
  LynxTestBenchRecorderInitConfig("/tmp", -1, 390, 844, record_id);
  rapidjson::Value params(rapidjson::kObjectType);
  ark.RecordInvokedMethodData("ExampleBridge", "invoke", params, record_id);
  wait(ark.thread_);

  ASSERT_TRUE(ark.replay_config_map_[record_id].IsObject());
  EXPECT_EQ(ark.GetRecordedFile(record_id)[kInvokedMethodData].Size(), 1);
  ark.Clear();
}

TEST(TestBenchBaseRecorder, ExternalTemplateBridgeUsesReplayLoadTemplateShape) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t record_id = 90214;
  constexpr char kUrl[] = "argus://microapp/test/pages/index-template.js";
  constexpr char kTemplate[] = "abc\0def";
  ark.Clear();
  ark.StartRecord();

  LynxTestBenchRecordExternalTemplateWithSize(0, kUrl, kTemplate,
                                              sizeof(kTemplate) - 1);
  LynxTestBenchRecordExternalTemplateWithSize(record_id, "", kTemplate,
                                              sizeof(kTemplate) - 1);
  LynxTestBenchRecordExternalTemplateWithSize(record_id, kUrl, kTemplate,
                                              sizeof(kTemplate) - 1);
  wait(ark.thread_);

  rapidjson::Value& action_list = ark.GetRecordedFile(record_id)[kActionList];
  ASSERT_EQ(action_list.Size(), 1);
  rapidjson::Value& action = action_list[0];
  EXPECT_STREQ(action[kFunctionName].GetString(), "loadTemplate");
  rapidjson::Value& params = action[kParams];
  EXPECT_STREQ(params["url"].GetString(), kUrl);
  EXPECT_STREQ(params["source"].GetString(), "YWJjAGRlZg==");
  ASSERT_TRUE(params["templateData"].IsObject());
  EXPECT_EQ(params["templateData"].MemberCount(), 0);
  EXPECT_TRUE(params["isCSR"].GetBool());

  ark.Clear();
}

TEST(TestBenchBaseRecorder, SetRecorderPath) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  ark.SetRecorderPath("/your/local/path");
  EXPECT_STREQ(ark.file_path_.c_str(), "/your/local/path/");
}

TEST(TestBenchBaseRecorder, SetScreenSize) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  int64_t record_id = 1;
  ark.SetScreenSize(record_id, 123, 456);
  wait(ark.thread_);
  EXPECT_EQ(ark.replay_config_map_.size(), 1);

  rapidjson::Value& config = ark.replay_config_map_[record_id];
  ASSERT_TRUE(config.IsObject());
  EXPECT_EQ(config.MemberCount(), 4);

  rapidjson::Value& jsb_ignored_info = config["jsbIgnoredInfo"];
  ASSERT_TRUE(jsb_ignored_info.IsArray());
  EXPECT_EQ(jsb_ignored_info.Size(), 0);

  rapidjson::Value& jsb_settings = config["jsbSettings"];
  ASSERT_TRUE(jsb_settings.IsObject());
  EXPECT_EQ(jsb_settings.MemberCount(), 1);

  rapidjson::Value& strict = jsb_settings["strict"];
  ASSERT_TRUE(strict.IsBool());
  ASSERT_TRUE(strict.GetBool());
}

TEST(TestBenchBaseRecorder, StartRecord) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  ark.StartRecord();
  ASSERT_TRUE(ark.is_recording_);
}

TEST(TestBenchBaseRecorder, RecordDebugInfo) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  int64_t record_id = 1;
  rapidjson::Value& debug_info_array =
      ark.GetRecordedFile(record_id)[kDebugInfo];
  EXPECT_EQ(debug_info_array.Size(), 0);
  ark.StartRecord();

  const char* test_url = "debug/test_url";
  const char* test_content =
      "Sample debug information with special characters: test content 🚀";

  ark.RecordDebugInfo(record_id, test_url, test_content);
  wait(ark.thread_);

  debug_info_array = ark.GetRecordedFile(record_id)[kDebugInfo];
  ASSERT_TRUE(debug_info_array.IsArray());
  EXPECT_EQ(debug_info_array.Size(), 1);

  rapidjson::Value& debug_entry = debug_info_array[0];
  ASSERT_TRUE(debug_entry.IsObject());
  EXPECT_STREQ(debug_entry[kParamDebugInfoUrl].GetString(), test_url);

  std::string encoded_content = debug_entry[kParamContent].GetString();
  EXPECT_FALSE(encoded_content.empty());
  EXPECT_EQ(
      encoded_content,
      "eJwFwcERgCAMBMBWrg7bsIIYg2QGAgPn+"
      "LUOP7ZoCe6uUnsx7LadBzxSG1XoLXA5M2Y3dSnQLEOUNuYC2iS0BS2I733uH2JDGms=");

  ark.Clear();
}

TEST(TestBenchBaseRecorder, RecordAction) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  int64_t record_id = 1;
  rapidjson::Value& action_list = ark.GetRecordedFile(record_id)[kActionList];
  EXPECT_EQ(action_list.Size(), 0);
  ark.StartRecord();
  rapidjson::Value params(rapidjson::kObjectType);

  ark.RecordAction("TestFunction", params, record_id);
  ark.RecordAction("TestGlobalFunction", params, record_id);

  wait(ark.thread_);

  EXPECT_EQ(action_list.Size(), 2);
  rapidjson::Value& test_action = action_list[0];
  ASSERT_TRUE(test_action.IsObject());
  EXPECT_EQ(test_action.MemberCount(), 4);
  rapidjson::Value& function_name = test_action[kFunctionName];
  EXPECT_STREQ(function_name.GetString(), "TestFunction");

  ASSERT_TRUE(test_action.HasMember(kParamRecordTime));
  ASSERT_TRUE(test_action.HasMember(kParamRecordMillisecond));
  ASSERT_TRUE(test_action.HasMember(kParams));

  rapidjson::Value& recorded_params = test_action[kParams];
  ASSERT_TRUE(recorded_params.IsObject());

  test_action = action_list[1];
  ASSERT_TRUE(test_action.IsObject());
  EXPECT_EQ(test_action.MemberCount(), 4);
  function_name = test_action[kFunctionName];
  EXPECT_STREQ(function_name.GetString(), "TestGlobalFunction");

  ASSERT_TRUE(test_action.HasMember(kParamRecordTime));
  ASSERT_TRUE(test_action.HasMember(kParamRecordMillisecond));
  ASSERT_TRUE(test_action.HasMember(kParams));

  recorded_params = test_action[kParams];
  ASSERT_TRUE(recorded_params.IsObject());

  ark.Clear();
}

TEST(TestBenchBaseRecorder, RecordInvokedMethodData) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  int record_id = 1;
  ark.StartRecord();
  ark.CreateRecordedFile(record_id);
  rapidjson::Value& invoked_method_data =
      ark.GetRecordedFileField(record_id, kInvokedMethodData);
  EXPECT_EQ(invoked_method_data.Size(), 0);
  rapidjson::Value params(rapidjson::kObjectType);
  ark.RecordInvokedMethodData("bridge", "call", params, record_id);
  wait(ark.thread_);
  EXPECT_EQ(invoked_method_data.Size(), 1);
  rapidjson::Value& invoked_module = invoked_method_data[0];
  ASSERT_TRUE(invoked_module.IsObject());
  EXPECT_EQ(invoked_module.MemberCount(), 5);
  ASSERT_TRUE(invoked_module.HasMember(kModuleName));
  EXPECT_STREQ(invoked_module[kModuleName].GetString(), "bridge");

  ASSERT_TRUE(invoked_module.HasMember(kMethodName));
  EXPECT_STREQ(invoked_module[kMethodName].GetString(), "call");

  ASSERT_TRUE(invoked_module.HasMember(kParamRecordTime));
  ASSERT_TRUE(invoked_module.HasMember(kParamRecordMillisecond));
  ASSERT_TRUE(invoked_module.HasMember(kParams));
  ASSERT_TRUE(invoked_module[kParams].IsObject());

  ark.Clear();
}

TEST(TestBenchBaseRecorder, RecordCallback) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  int64_t record_id = 1;
  int64_t callback_id = 2;
  ark.StartRecord();
  ark.CreateRecordedFile(record_id);
  rapidjson::Value& callback = ark.GetRecordedFileField(record_id, kCallback);
  EXPECT_EQ(callback.MemberCount(), 0);
  rapidjson::Value params(rapidjson::kObjectType);
  ark.RecordCallback("bridge", "call", params, callback_id, record_id);
  wait(ark.thread_);
  EXPECT_EQ(callback.MemberCount(), 1);
  ASSERT_TRUE(callback.HasMember("2"));
  rapidjson::Value& callback_info = callback["2"];

  ASSERT_TRUE(callback_info.IsObject());
  EXPECT_EQ(callback_info.MemberCount(), 5);
  ASSERT_TRUE(callback_info.HasMember(kModuleName));
  EXPECT_STREQ(callback_info[kModuleName].GetString(), "bridge");

  ASSERT_TRUE(callback_info.HasMember(kMethodName));
  EXPECT_STREQ(callback_info[kMethodName].GetString(), "call");

  ASSERT_TRUE(callback_info.HasMember(kParamRecordTime));
  ASSERT_TRUE(callback_info.HasMember(kParamRecordMillisecond));
  ASSERT_TRUE(callback_info.HasMember(kParams));
  ASSERT_TRUE(callback_info[kParams].IsObject());
  ark.Clear();
}

TEST(TestBenchBaseRecorder, PreservesRepeatedCallbackIDs) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t kRecordId = 3;
  constexpr int64_t kCallbackId = 9;
  ark.StartRecord();
  ark.CreateRecordedFile(kRecordId);
  rapidjson::Value first_params(rapidjson::kObjectType);
  rapidjson::Value second_params(rapidjson::kObjectType);

  ark.RecordCallback("bridge", "call", first_params, kCallbackId, kRecordId);
  ark.RecordCallback("bridge", "call", second_params, kCallbackId, kRecordId);
  wait(ark.thread_);

  rapidjson::Value& callback = ark.GetRecordedFileField(kRecordId, kCallback);
  ASSERT_EQ(callback.MemberCount(), 1);
  ASSERT_TRUE(callback["9"].IsArray());
  EXPECT_EQ(callback["9"].Size(), 2);
  ark.Clear();
}

TEST(TestBenchBaseRecorder, RecorderControllerPreservesLegacyABI) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  constexpr int64_t kRecordId = 7;
  constexpr int64_t kCallbackId = 8;
  constexpr char kParams[] = R"({"returnValue":"ok"})";

  ark.Clear();
  ark.StartRecord();

  EXPECT_TRUE(LynxTestBenchRecorderRecordInvokedMethod(
      kRecordId, "ExampleBridge", "invoke", kParams));
  uint64_t generation = LynxTestBenchRecorderRecordInvokedMethodWithGeneration(
      kRecordId, "ExampleBridge", "invoke", kParams);
  EXPECT_EQ(generation, ark.RecordingGeneration());
  EXPECT_TRUE(LynxTestBenchRecorderRecordCallback(
      kRecordId, "ExampleBridge", "invoke", kCallbackId, kParams));
  EXPECT_TRUE(LynxTestBenchRecorderRecordCallbackWithGeneration(
      kRecordId, "ExampleBridge", "invoke", kCallbackId + 1, kParams,
      generation));

  wait(ark.thread_);
  EXPECT_EQ(ark.GetRecordedFile(kRecordId)[kInvokedMethodData].Size(), 2);
  EXPECT_EQ(ark.GetRecordedFile(kRecordId)[kCallback].MemberCount(), 2);
  ark.Clear();
}

TEST(TestBenchBaseRecorder, RecordComponent) {
  TestBenchBaseRecorder& ark = TestBenchBaseRecorder::GetInstance();
  int64_t record_id = 1;
  int64_t type = 66;
  const char* component_name = "test-ui";
  ark.StartRecord();
  rapidjson::Value& component_list =
      ark.GetRecordedFileField(record_id, kComponentList);
  EXPECT_EQ(component_list.Size(), 0);

  ark.RecordComponent(component_name, type, record_id);
  wait(ark.thread_);
  EXPECT_EQ(component_list.Size(), 1);
  rapidjson::Value& component = component_list[0];
  ASSERT_TRUE(component.IsObject());
  EXPECT_EQ(component.MemberCount(), 2);
  ASSERT_TRUE(component.HasMember(kComponentName));
  EXPECT_STREQ(component[kComponentName].GetString(), component_name);
  ASSERT_TRUE(component.HasMember(kComponentType));
  EXPECT_EQ(component[kComponentType].GetInt(), type);
  ark.Clear();
}

}  // namespace recorder
}  // namespace tasm
}  // namespace lynx
