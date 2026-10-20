// Manual checks of the updater and the content installers (not run by ctest, not shipped).
// Usage: launcher_update_probe <cd1|cd2> [<package folder> <tag>]   lists one game's GitHub releases and optionally
//                                                                     installs one, as the Updates dialog does
//        launcher_update_probe --title-update <cd2 folder> <game folder> <package>
//        launcher_update_probe --dlc <save folder> <package>          Crackdown 2 packages, as the launcher installs them
#include "content.h"
#include "games.h"
#include <iostream>
using namespace launcher;
int main(int argc,char** argv) {
  std::atomic_bool cancel=false; const auto& cd2=kGames[game_index("cd2")];
  try {
    if(argc==5&&std::string(argv[1])=="--title-update") {
      std::cout<<install_content_update(fs::u8path(argv[4]),fs::u8path(argv[2]),fs::u8path(argv[3]),cd2.title_id,cd2.update_xexp,cancel)<<" installed\n";
      return 0;
    }
    if(argc==4&&std::string(argv[1])=="--dlc") {
      auto pack=install_content_dlc(fs::u8path(argv[3]),fs::u8path(argv[2]),cd2.title_id,cancel);
      std::cout<<pack.title<<" installed as "<<pack.file_name<<'\n';
      for(const auto& p:list_content_dlc(fs::u8path(argv[2]),cd2.title_id)) std::cout<<"  listed: "<<p.title<<'\n';
      return 0;
    }
    if(argc!=2&&argc!=4) { std::cerr<<"usage: see update_probe.cpp\n"; return 2; }
    const auto& g=kGames[game_index(argv[1])];
    auto releases=check_releases(true,cancel,*g.updates);
    for(const auto& r:releases) std::cout<<r.tag<<(r.prerelease?" (prerelease) ":" ")<<(r.zip_url.empty()?"no package":r.zip_url)<<'\n';
    if(argc==4) {
      fs::path package=fs::absolute(fs::u8path(argv[2]));
      for(const auto& r:releases) if(r.tag==argv[3]) {
        install_release(r,package,cancel,*g.updates);
        std::cout<<"installed "<<r.tag<<"; installed_version="<<installed_version(game_home(package,g),*g.updates)<<'\n'; return 0;
      }
      std::cerr<<"no release "<<argv[3]<<'\n'; return 1;
    }
    return 0;
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
