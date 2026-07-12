#pragma once

#include <wx/dialog.h>

#include "core/project.h"

namespace say_count::ui {

enum class ConflictDialogChoice { Cancel, KeepLocal, UseDisk, SaveElsewhere };

class ConflictDialog final : public wxDialog {
public:
    ConflictDialog(wxWindow* parent, const ExternalConflict& conflict);
    ConflictDialogChoice choice() const { return choice_; }

private:
    ConflictDialogChoice choice_ = ConflictDialogChoice::Cancel;
};

}  // namespace say_count::ui
