#pragma once

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <string>
#include <vector>

namespace lal
{
    /**
     * \brief Helper class that holds some data describing the current formatting state. 
     */
    class FormatState
    {
    public:
        FormatState() = default;

        FormatState(uint32_t indent, char character);

        void pushRegion(std::string name);

        std::string popRegion();

        std::string& getRegionPrepend();

    private:
        uint32_t                 indent    = 0;
        char                     character = ' ';
        std::vector<std::string> regionStack;
        std::string              regionPrepend;
    };
}  // namespace lal
