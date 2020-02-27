#include <log.h>

class Environment : public ::testing::Environment {
public:
  void SetUp() override
  {
    MOBase::log::createDefault(MOBase::log::LoggerConfiguration());
  }
};


int main(int argc, char **argv)
{
  ::testing::AddGlobalTestEnvironment(new Environment);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
