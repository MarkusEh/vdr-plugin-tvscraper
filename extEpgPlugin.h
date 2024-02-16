#ifndef __TVSCRAPER_EXTEPGPLUGIN_H
#define __TVSCRAPER_EXTEPGPLUGIN_H
// interface for external epg plugin

#include "services.h"
#include <vdr/epg.h>
#include "sEvent.h"
#include <string>
#include <vector>
#include <memory>

//
// to be implemented by the extEpg plugin ===========================
//

class iExtEpgForChannel
{
  public:
    iExtEpgForChannel() {}
    virtual ~iExtEpgForChannel() {}
    virtual void enhanceEvent(cStaticEvent *event, std::vector<cTvMedia> &extEpgImages) = 0;
};
class iExtEpg
{
  public:
    iExtEpg() {}
    virtual ~iExtEpg() {}
    virtual const char *getSource() = 0;
    virtual bool myDescription(const char *description) = 0;
    virtual std::shared_ptr<iExtEpgForChannel> getHandlerForChannel(const std::string &extChannelId) = 0;
};

extern "C" iExtEpg* extEpgPluginCreator(bool debug);

#endif // __TVSCRAPER_EXTEPGPLUGIN_H
