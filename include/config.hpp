#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <filesystem>
#include <map>
#include <vector>

class Config {
 public:
  Config(const std::string& cfg_path);

  std::map<std::string, std::string> get_games();
  std::string get_profiles_dir();
  std::vector<std::string> get_thread_name_list(const std::string& game_name);

  template <typename T>
  T get_key(const std::string& key, T default_value) {
    return root.get<T>(key, default_value);
  }

  template <typename T>
  std::vector<T> get_key_array(const std::string& key) {
    std::vector<T> v;

    try {
      for (auto& p : root.get_child(key)) {
        v.push_back(p.second.get<T>(""));
      }
    } catch (const boost::property_tree::ptree_error& e) {
    }

    return v;
  }

  template <typename T>
  T get_profile_key(const std::string& game_name, const std::string& key, T default_value) {
    boost::property_tree::ptree r;

    boost::property_tree::read_json(profiles_dir + "/" + games[game_name], r);

    return r.get<T>(key, default_value);
  }

  template <typename T>
  std::vector<T> get_profile_key_array(const std::string& game_name, const std::string& key) {
    std::vector<T> v;

    boost::property_tree::ptree r;

    boost::property_tree::read_json(profiles_dir + "/" + games[game_name], r);

    try {
      for (auto& p : r.get_child(key)) {
        v.push_back(p.second.get<T>(""));
      }
    } catch (const boost::property_tree::ptree_error& e) {
    }

    return v;
  }

 private:
  std::string log_tag = "config: ";

  boost::property_tree::ptree root;

  std::map<std::string, std::string> games;

  std::string profiles_dir;
};

#endif