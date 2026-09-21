import dynamips;

int main() {
  return dynamips::message(dynamips::error::internal).empty() ? 1 : 0;
}
