# application 層：use case 與 port。只依賴 domain 與標準庫（不用 Qt、OpenCV、HALCON）。

AOI_MODULE = application
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/application
SOURCES += \
    $$DIR/backend/BackendRegistry.cpp \
    $$DIR/backend/BackendSwitch.cpp \
    $$DIR/backend/SearchEngine.cpp \
    $$DIR/compare/BackendComparison.cpp \
    $$DIR/document/InspectionDocument.cpp \
    $$DIR/document/ProposalSession.cpp \
    $$DIR/document/RecipeSaver.cpp \
    $$DIR/image/ImageSourceChain.cpp \
    $$DIR/interaction/InspectionSession.cpp \
    $$DIR/interaction/InteractionStateMachine.cpp \
    $$DIR/jobs/JobRunner.cpp \
    $$DIR/usecases/BoardProposal.cpp \
    $$DIR/usecases/GenerateObjectsFromSample.cpp \
    $$DIR/usecases/ProposalView.cpp \
    $$DIR/usecases/SearchBoardWithSamples.cpp

HEADERS += \
    $$DIR/CancelToken.h \
    $$DIR/Error.h \
    $$DIR/Result.h \
    $$DIR/backend/BackendRegistry.h \
    $$DIR/backend/BackendSwitch.h \
    $$DIR/backend/SearchEngine.h \
    $$DIR/compare/BackendComparison.h \
    $$DIR/document/InspectionDocument.h \
    $$DIR/document/ProposalSession.h \
    $$DIR/document/RecipeSaver.h \
    $$DIR/document/UndoStack.h \
    $$DIR/image/ImageSourceChain.h \
    $$DIR/interaction/InspectionSession.h \
    $$DIR/interaction/InteractionStateMachine.h \
    $$DIR/jobs/JobRunner.h \
    $$DIR/ports/IExecutor.h \
    $$DIR/ports/IImageSource.h \
    $$DIR/ports/ILibraryMatcher.h \
    $$DIR/ports/ILibraryStore.h \
    $$DIR/ports/IPatternSearcher.h \
    $$DIR/ports/IPerfProbe.h \
    $$DIR/ports/IProgressSink.h \
    $$DIR/ports/IRecipeStore.h \
    $$DIR/ports/ITemplateBuilder.h \
    $$DIR/usecases/BoardProposal.h \
    $$DIR/usecases/GenerateObjectsFromSample.h \
    $$DIR/usecases/ProposalView.h \
    $$DIR/usecases/SearchBoardWithSamples.h
