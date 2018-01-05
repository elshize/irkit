#pragma once

#include "../index.hpp"

namespace bloodhound::index::format {

struct TermDetails {
    TermId term;
    Offset doc_offset;
    Offset score_offset;
};

template<typename Value>
struct PostingBlock {
    Value head;
    RelativeOffset next_block;
};

};  // namespace bloodhound::index::format
