#include "ArmGeometry.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::string Trim(const std::string& input) {
  const size_t first = input.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = input.find_last_not_of(" \t\r\n");
  return input.substr(first, last - first + 1);
}

std::vector<std::string> SplitCsvLine(const std::string& line) {
  std::vector<std::string> columns;
  std::stringstream stream(line);
  std::string item;
  while (std::getline(stream, item, ',')) {
    columns.push_back(Trim(item));
  }
  while (columns.size() < 17) {
    columns.push_back("");
  }
  return columns;
}

double ParseNumericOrSymbol(std::string token,
                            const std::map<std::string, double>& dimensions) {
  token = Trim(token);
  if (token.empty()) {
    throw std::runtime_error("Empty numeric token in arm geometry file");
  }

  size_t parsed_chars = 0;
  try {
    const double value = std::stod(token, &parsed_chars);
    if (parsed_chars == token.size()) {
      return value;
    }
  } catch (const std::exception&) {
  }

  const size_t multiply_pos = token.find('*');
  if (multiply_pos != std::string::npos) {
    const double lhs = ParseNumericOrSymbol(token.substr(0, multiply_pos),
                                            dimensions);
    const double rhs = ParseNumericOrSymbol(token.substr(multiply_pos + 1),
                                            dimensions);
    return lhs * rhs;
  }

  double sign = 1.0;
  if (!token.empty() && token.front() == '-') {
    sign = -1.0;
    token = token.substr(1);
  } else if (!token.empty() && token.front() == '+') {
    token = token.substr(1);
  }

  const auto it = dimensions.find(token);
  if (it == dimensions.end()) {
    throw std::runtime_error("Unknown dimension symbol: " + token);
  }
  return sign * it->second;
}

}  // namespace

namespace arm_geometry {

ArmGeometry LoadArmGeometry(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("Could not open arm geometry file: " + path);
  }

  ArmGeometry geometry;
  std::string line;
  std::getline(file, line);

  while (std::getline(file, line)) {
    if (Trim(line).empty()) {
      continue;
    }

    const std::vector<std::string> c = SplitCsvLine(line);
    const std::string record_type = c[0];
    if (record_type == "dimension") {
      geometry.dimensions[c[1]] = std::stod(c[2]);
      continue;
    }

    if (record_type == "joint") {
      geometry.joints.push_back({
          c[1],
          ParseNumericOrSymbol(c[3], geometry.dimensions),
          ParseNumericOrSymbol(c[4], geometry.dimensions),
          ParseNumericOrSymbol(c[5], geometry.dimensions),
          ParseNumericOrSymbol(c[6], geometry.dimensions),
          ParseNumericOrSymbol(c[7], geometry.dimensions),
          ParseNumericOrSymbol(c[8], geometry.dimensions),
          ParseNumericOrSymbol(c[9], geometry.dimensions),
      });
      continue;
    }

    if (record_type == "apriltag") {
      geometry.april_tags.push_back({
          c[1],
          std::stoi(c[10]),
          {{ParseNumericOrSymbol(c[11], geometry.dimensions),
            ParseNumericOrSymbol(c[12], geometry.dimensions),
            ParseNumericOrSymbol(c[13], geometry.dimensions)},
           {ParseNumericOrSymbol(c[14], geometry.dimensions),
            ParseNumericOrSymbol(c[15], geometry.dimensions),
            ParseNumericOrSymbol(c[16], geometry.dimensions)}},
      });
      continue;
    }

    throw std::runtime_error("Unknown arm geometry record type: " + record_type);
  }

  if (geometry.joints.size() != 6) {
    throw std::runtime_error("The POC expects exactly 6 robot joints");
  }
  if (geometry.april_tags.size() != 8) {
    throw std::runtime_error("The POC expects exactly 8 AprilTags");
  }
  return geometry;
}

std::vector<double> ClampJointAngles(
    const ArmGeometry& geometry, const std::vector<double>& joint_angles_deg) {
  std::vector<double> out = joint_angles_deg;
  for (size_t i = 0; i < out.size() && i < geometry.joints.size(); ++i) {
    out[i] = std::min(std::max(out[i], geometry.joints[i].min_deg),
                      geometry.joints[i].max_deg);
  }
  return out;
}

std::vector<double> InitialJointAngles(const ArmGeometry& geometry) {
  std::vector<double> out;
  out.reserve(geometry.joints.size());
  for (const JointGeometry& joint : geometry.joints) {
    out.push_back(joint.initial_deg);
  }
  return out;
}

}  // namespace arm_geometry
