#ifndef TOKENCORE_QT_INIT_H
#define TOKENCORE_QT_INIT_H

namespace TokenCore
{
    //! Shows an user dialog with general warnings and potential risks
    bool AskUserToAcknowledgeRisks();

    //! Setup and initialization related to Token Core Qt
    bool Initialize();
}

#endif // TOKENCORE_QT_INIT_H
