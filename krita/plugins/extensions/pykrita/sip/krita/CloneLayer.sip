%ModuleHeaderCode
#include "CloneLayer.h"
%End

class CloneLayer : Node
{
%TypeHeaderCode
#include "CloneLayer.h"
%End

%ConvertToSubClassCode
    if(qobject_cast<CloneLayer*>(sipCpp))
        sipType = sipType_CloneLayer;
    else
        sipType = nullptr;
%End

    CloneLayer(const CloneLayer & __0);
public:
    virtual ~CloneLayer();
    virtual QString type() const;
    Node* sourceNode() const /Factory/;
    void setSourceNode(Node* node);
Q_SIGNALS:
private:
};
