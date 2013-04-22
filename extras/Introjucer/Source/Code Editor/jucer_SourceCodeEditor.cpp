/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-11 by Raw Material Software Ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the GNU General
   Public License (Version 2), as published by the Free Software Foundation.
   A copy of the license is included in the JUCE distribution, or can be found
   online at www.gnu.org/licenses.

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.rawmaterialsoftware.com/juce for more information.

  ==============================================================================
*/

#include "jucer_SourceCodeEditor.h"
#include "../Application/jucer_OpenDocumentManager.h"


//==============================================================================
SourceCodeDocument::SourceCodeDocument (Project* p, const File& f)
    : modDetector (f), project (p)
{
}

CodeDocument& SourceCodeDocument::getCodeDocument()
{
    if (codeDoc == nullptr)
    {
        codeDoc = new CodeDocument();
        reloadInternal();
        codeDoc->clearUndoHistory();
    }

    return *codeDoc;
}

Component* SourceCodeDocument::createEditor()
{
    SourceCodeEditor* e = new SourceCodeEditor (this);
    e->createEditor (getCodeDocument());
    applyLastState (*(e->editor));
    return e;
}

void SourceCodeDocument::reloadFromFile()
{
    getCodeDocument();
    reloadInternal();
}

void SourceCodeDocument::reloadInternal()
{
    jassert (codeDoc != nullptr);
    modDetector.updateHash();
    codeDoc->applyChanges (getFile().loadFileAsString());
    codeDoc->setSavePoint();
}

static bool writeCodeDocToFile (const File& file, CodeDocument& doc)
{
    TemporaryFile temp (file);

    {
        FileOutputStream fo (temp.getFile());

        if (! (fo.openedOk() && doc.writeToStream (fo)))
            return false;
    }

    return temp.overwriteTargetFileWithTemporary();
}

bool SourceCodeDocument::save()
{
    if (writeCodeDocToFile (getFile(), getCodeDocument()))
    {
        getCodeDocument().setSavePoint();
        modDetector.updateHash();
        return true;
    }

    return false;
}

bool SourceCodeDocument::saveAs()
{
    FileChooser fc (TRANS("Save As..."), getFile(), "*");

    if (! fc.browseForFileToSave (true))
        return true;

    return writeCodeDocToFile (fc.getResult(), getCodeDocument());
}

void SourceCodeDocument::updateLastState (CodeEditorComponent& editor)
{
    lastState = new CodeEditorComponent::State (editor);
}

void SourceCodeDocument::applyLastState (CodeEditorComponent& editor) const
{
    if (lastState != nullptr)
        lastState->restoreState (editor);
}

//==============================================================================
SourceCodeEditor::SourceCodeEditor (OpenDocumentManager::Document* doc)
    : DocumentEditorComponent (doc)
{
}

SourceCodeEditor::~SourceCodeEditor()
{
    if (editor != nullptr)
        editor->getDocument().removeListener (this);

    getAppSettings().appearance.settings.removeListener (this);

    if (SourceCodeDocument* doc = dynamic_cast <SourceCodeDocument*> (getDocument()))
        doc->updateLastState (*editor);
}

void SourceCodeEditor::createEditor (CodeDocument& codeDocument)
{
    if (document->getFile().hasFileExtension (sourceOrHeaderFileExtensions))
        setEditor (new CppCodeEditorComponent (document->getFile(), codeDocument));
    else
        setEditor (new GenericCodeEditorComponent (document->getFile(), codeDocument, nullptr));
}

void SourceCodeEditor::setEditor (CodeEditorComponent* newEditor)
{
    if (editor != nullptr)
        editor->getDocument().removeListener (this);

    addAndMakeVisible (editor = newEditor);

    editor->setFont (AppearanceSettings::getDefaultCodeFont());
    editor->setTabSize (4, true);

    updateColourScheme();
    getAppSettings().appearance.settings.addListener (this);

    editor->getDocument().addListener (this);
}

void SourceCodeEditor::scrollToKeepRangeOnScreen (const Range<int>& range)
{
    const int space = jmin (10, editor->getNumLinesOnScreen() / 3);
    const CodeDocument::Position start (editor->getDocument(), range.getStart());
    const CodeDocument::Position end   (editor->getDocument(), range.getEnd());

    editor->scrollToKeepLinesOnScreen (Range<int> (start.getLineNumber() - space, end.getLineNumber() + space));
}

void SourceCodeEditor::highlight (const Range<int>& range, bool cursorAtStart)
{
    scrollToKeepRangeOnScreen (range);

    if (cursorAtStart)
    {
        editor->moveCaretTo (CodeDocument::Position (editor->getDocument(), range.getEnd()),   false);
        editor->moveCaretTo (CodeDocument::Position (editor->getDocument(), range.getStart()), true);
    }
    else
    {
        editor->setHighlightedRegion (range);
    }
}

void SourceCodeEditor::resized()
{
    editor->setBounds (getLocalBounds());
}

void SourceCodeEditor::updateColourScheme()     { getAppSettings().appearance.applyToCodeEditor (*editor); }

void SourceCodeEditor::checkSaveState()
{
    setEditedState (getDocument()->needsSaving());
}

void SourceCodeEditor::valueTreePropertyChanged (ValueTree&, const Identifier&)   { updateColourScheme(); }
void SourceCodeEditor::valueTreeChildAdded (ValueTree&, ValueTree&)               { updateColourScheme(); }
void SourceCodeEditor::valueTreeChildRemoved (ValueTree&, ValueTree&)             { updateColourScheme(); }
void SourceCodeEditor::valueTreeChildOrderChanged (ValueTree&)                    { updateColourScheme(); }
void SourceCodeEditor::valueTreeParentChanged (ValueTree&)                        { updateColourScheme(); }
void SourceCodeEditor::valueTreeRedirected (ValueTree&)                           { updateColourScheme(); }

void SourceCodeEditor::codeDocumentTextInserted (const String&, int)              { checkSaveState(); }
void SourceCodeEditor::codeDocumentTextDeleted (int, int)                         { checkSaveState(); }

//==============================================================================
GenericCodeEditorComponent::GenericCodeEditorComponent (const File& f, CodeDocument& codeDocument,
                                                        CodeTokeniser* tokeniser)
   : CodeEditorComponent (codeDocument, tokeniser), file (f)
{
    setCommandManager (commandManager);
}

GenericCodeEditorComponent::~GenericCodeEditorComponent() {}

enum
{
    showInFinderID = 0x2fe821e3,
    insertComponentID = 0x2fe821e4
};

void GenericCodeEditorComponent::addPopupMenuItems (PopupMenu& menu, const MouseEvent* e)
{
    menu.addItem (showInFinderID,
                 #if JUCE_MAC
                  "Reveal " + file.getFileName() + " in Finder");
                 #else
                  "Reveal " + file.getFileName() + " in Explorer");
                 #endif
    menu.addSeparator();

    CodeEditorComponent::addPopupMenuItems (menu, e);
}

void GenericCodeEditorComponent::performPopupMenuAction (int menuItemID)
{
    if (menuItemID == showInFinderID)
        file.revealToUser();
    else
        CodeEditorComponent::performPopupMenuAction (menuItemID);
}

void GenericCodeEditorComponent::getAllCommands (Array <CommandID>& commands)
{
    CodeEditorComponent::getAllCommands (commands);

    const CommandID ids[] = { CommandIDs::showFindPanel,
                              CommandIDs::findSelection,
                              CommandIDs::findNext,
                              CommandIDs::findPrevious };

    commands.addArray (ids, numElementsInArray (ids));
}

void GenericCodeEditorComponent::getCommandInfo (const CommandID commandID, ApplicationCommandInfo& result)
{
    const bool anythingSelected = isHighlightActive();

    switch (commandID)
    {
        case CommandIDs::showFindPanel:
            result.setInfo (TRANS ("Find"), TRANS ("Searches for text in the current document."), "Editing", 0);
            result.defaultKeypresses.add (KeyPress ('f', ModifierKeys::commandModifier, 0));
            break;

        case CommandIDs::findSelection:
            result.setInfo (TRANS ("Find Selection"), TRANS ("Searches for the currently selected text."), "Editing", 0);
            result.setActive (anythingSelected);
            result.defaultKeypresses.add (KeyPress ('l', ModifierKeys::commandModifier, 0));
            break;

        case CommandIDs::findNext:
            result.setInfo (TRANS ("Find Next"), TRANS ("Searches for the next occurrence of the current search-term."), "Editing", 0);
            result.defaultKeypresses.add (KeyPress ('g', ModifierKeys::commandModifier, 0));
            break;

        case CommandIDs::findPrevious:
            result.setInfo (TRANS ("Find Previous"), TRANS ("Searches for the previous occurrence of the current search-term."), "Editing", 0);
            result.defaultKeypresses.add (KeyPress ('g', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0));
            result.defaultKeypresses.add (KeyPress ('d', ModifierKeys::commandModifier, 0));
            break;

        default:
            CodeEditorComponent::getCommandInfo (commandID, result);
            break;
    }
}

bool GenericCodeEditorComponent::perform (const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case CommandIDs::showFindPanel:     showFindPanel();         return true;
        case CommandIDs::findSelection:     findSelection();         return true;
        case CommandIDs::findNext:          findNext (true, true);   return true;
        case CommandIDs::findPrevious:      findNext (false, false); return true;
        default:                            break;
    }

    return CodeEditorComponent::perform (info);
}

//==============================================================================
class GenericCodeEditorComponent::FindPanel  : public Component,
                                               private TextEditor::Listener,
                                               private ButtonListener
{
public:
    FindPanel()
        : caseButton ("Case-sensitive"),
          findPrev ("<"),
          findNext (">")
    {
        editor.setColour (CaretComponent::caretColourId, Colours::black);

        addAndMakeVisible (&editor);
        label.setText ("Find:", dontSendNotification);
        label.setColour (Label::textColourId, Colours::white);
        label.attachToComponent (&editor, false);

        addAndMakeVisible (&caseButton);
        caseButton.setColour (ToggleButton::textColourId, Colours::white);
        caseButton.setToggleState (isCaseSensitiveSearch(), false);
        caseButton.addListener (this);

        findPrev.setConnectedEdges (Button::ConnectedOnRight);
        findNext.setConnectedEdges (Button::ConnectedOnLeft);
        addAndMakeVisible (&findPrev);
        addAndMakeVisible (&findNext);

        setWantsKeyboardFocus (false);
        setFocusContainer (true);
        findPrev.setWantsKeyboardFocus (false);
        findNext.setWantsKeyboardFocus (false);

        editor.setText (getSearchString());
        editor.addListener (this);
    }

    void setCommandManager (ApplicationCommandManager* cm)
    {
        findPrev.setCommandToTrigger (cm, CommandIDs::findPrevious, true);
        findNext.setCommandToTrigger (cm, CommandIDs::findNext, true);
    }

    void paint (Graphics& g) override
    {
        Path outline;
        outline.addRoundedRectangle (1.0f, 1.0f, getWidth() - 2.0f, getHeight() - 2.0f, 8.0f);

        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillPath (outline);
        g.setColour (Colours::white.withAlpha (0.8f));
        g.strokePath (outline, PathStrokeType (1.0f));
    }

    void resized() override
    {
        int y = 30;
        editor.setBounds (10, y, getWidth() - 20, 24);
        y += 30;
        caseButton.setBounds (10, y, getWidth() / 2 - 10, 22);
        findNext.setBounds (getWidth() - 40, y, 30, 22);
        findPrev.setBounds (getWidth() - 70, y, 30, 22);
    }

    void buttonClicked (Button*) override
    {
        setCaseSensitiveSearch (caseButton.getToggleState());
    }

    void textEditorTextChanged (TextEditor&) override
    {
        setSearchString (editor.getText());

        if (GenericCodeEditorComponent* ed = getOwner())
            ed->findNext (true, false);
    }

    void textEditorFocusLost (TextEditor&) override {}

    void textEditorReturnKeyPressed (TextEditor&) override
    {
        commandManager->invokeDirectly (CommandIDs::findNext, true);
    }

    void textEditorEscapeKeyPressed (TextEditor&) override
    {
        if (GenericCodeEditorComponent* ed = getOwner())
            ed->hideFindPanel();
    }

    GenericCodeEditorComponent* getOwner() const
    {
        return findParentComponentOfClass <GenericCodeEditorComponent>();
    }

    TextEditor editor;
    Label label;
    ToggleButton caseButton;
    TextButton findPrev, findNext;
};

void GenericCodeEditorComponent::resized()
{
    CodeEditorComponent::resized();

    if (findPanel != nullptr)
    {
        findPanel->setSize (jmin (260, getWidth() - 32), 100);
        findPanel->setTopRightPosition (getWidth() - 16, 8);
    }
}

void GenericCodeEditorComponent::showFindPanel()
{
    if (findPanel == nullptr)
    {
        findPanel = new FindPanel();
        findPanel->setCommandManager (commandManager);

        addAndMakeVisible (findPanel);
        resized();
    }

    if (findPanel != nullptr)
    {
        findPanel->editor.grabKeyboardFocus();
        findPanel->editor.selectAll();
    }
}

void GenericCodeEditorComponent::hideFindPanel()
{
    findPanel = nullptr;
}

void GenericCodeEditorComponent::findSelection()
{
    const String selected (getTextInRange (getHighlightedRegion()));

    if (selected.isNotEmpty())
    {
        setSearchString (selected);
        findNext (true, true);
    }
}

void GenericCodeEditorComponent::findNext (bool forwards, bool skipCurrentSelection)
{
    const Range<int> highlight (getHighlightedRegion());
    const CodeDocument::Position startPos (getDocument(), skipCurrentSelection ? highlight.getEnd()
                                                                               : highlight.getStart());
    int lineNum = startPos.getLineNumber();
    int linePos = startPos.getIndexInLine();

    const int totalLines = getDocument().getNumLines();
    const String searchText (getSearchString());
    const bool caseSensitive = isCaseSensitiveSearch();

    for (int linesToSearch = totalLines; --linesToSearch >= 0;)
    {
        String line (getDocument().getLine (lineNum));
        int index;

        if (forwards)
        {
            index = caseSensitive ? line.indexOf (linePos, searchText)
                                  : line.indexOfIgnoreCase (linePos, searchText);
        }
        else
        {
            if (linePos >= 0)
                line = line.substring (0, linePos);

            index = caseSensitive ? line.lastIndexOf (searchText)
                                  : line.lastIndexOfIgnoreCase (searchText);
        }

        if (index >= 0)
        {
            const CodeDocument::Position p (getDocument(), lineNum, index);
            selectRegion (p, p.movedBy (searchText.length()));
            break;
        }

        if (forwards)
        {
            linePos = 0;
            lineNum = (lineNum + 1) % totalLines;
        }
        else
        {
            if (--lineNum < 0)
                lineNum = totalLines - 1;

            linePos = -1;
        }
    }
}

void GenericCodeEditorComponent::handleEscapeKey()
{
    CodeEditorComponent::handleEscapeKey();
    hideFindPanel();
}

//==============================================================================
static CPlusPlusCodeTokeniser cppTokeniser;

CppCodeEditorComponent::CppCodeEditorComponent (const File& f, CodeDocument& doc)
    : GenericCodeEditorComponent (f, doc, &cppTokeniser)
{
}

CppCodeEditorComponent::~CppCodeEditorComponent() {}

void CppCodeEditorComponent::handleReturnKey()
{
    GenericCodeEditorComponent::handleReturnKey();

    CodeDocument::Position pos (getCaretPos());

    String blockIndent, lastLineIndent;
    CodeHelpers::getIndentForCurrentBlock (pos, getTabString (getTabSize()), blockIndent, lastLineIndent);

    const String remainderOfBrokenLine (pos.getLineText());
    const int numLeadingWSChars = CodeHelpers::getLeadingWhitespace (remainderOfBrokenLine).length();

    if (numLeadingWSChars > 0)
        getDocument().deleteSection (pos, pos.movedBy (numLeadingWSChars));

    if (remainderOfBrokenLine.trimStart().startsWithChar ('}'))
        insertTextAtCaret (blockIndent);
    else
        insertTextAtCaret (lastLineIndent);

    const String previousLine (pos.movedByLines (-1).getLineText());
    const String trimmedPreviousLine (previousLine.trim());

    if ((trimmedPreviousLine.startsWith ("if ")
          || trimmedPreviousLine.startsWith ("if(")
          || trimmedPreviousLine.startsWith ("for ")
          || trimmedPreviousLine.startsWith ("for(")
          || trimmedPreviousLine.startsWith ("while(")
          || trimmedPreviousLine.startsWith ("while "))
         && trimmedPreviousLine.endsWithChar (')'))
    {
        insertTabAtCaret();
    }
}

void CppCodeEditorComponent::insertTextAtCaret (const String& newText)
{
    if (getHighlightedRegion().isEmpty())
    {
        const CodeDocument::Position pos (getCaretPos());

        if ((newText == "{" || newText == "}")
             && pos.getLineNumber() > 0
             && pos.getLineText().trim().isEmpty())
        {
            moveCaretToStartOfLine (true);

            String blockIndent, lastLineIndent;
            if (CodeHelpers::getIndentForCurrentBlock (pos, getTabString (getTabSize()), blockIndent, lastLineIndent))
            {
                GenericCodeEditorComponent::insertTextAtCaret (blockIndent);

                if (newText == "{")
                    insertTabAtCaret();
            }
        }
    }

    GenericCodeEditorComponent::insertTextAtCaret (newText);
}

void CppCodeEditorComponent::addPopupMenuItems (PopupMenu& menu, const MouseEvent* e)
{
    GenericCodeEditorComponent::addPopupMenuItems (menu, e);

    menu.addSeparator();
    menu.addItem (insertComponentID, TRANS("Insert code for a new Component class..."));
}

void CppCodeEditorComponent::performPopupMenuAction (int menuItemID)
{
    if (menuItemID == insertComponentID)
        insertComponentClass();

    GenericCodeEditorComponent::performPopupMenuAction (menuItemID);
}

void CppCodeEditorComponent::insertComponentClass()
{
    AlertWindow aw (TRANS ("Insert a new Component class"),
                    TRANS ("Please enter a name for the new class"),
                    AlertWindow::NoIcon, nullptr);

    const char* classNameField = "Class Name";

    aw.addTextEditor (classNameField, String::empty, String::empty, false);
    aw.addButton (TRANS ("Insert Code"),  1, KeyPress (KeyPress::returnKey));
    aw.addButton (TRANS ("Cancel"),       0, KeyPress (KeyPress::escapeKey));

    while (aw.runModalLoop() != 0)
    {
        const String className (aw.getTextEditorContents (classNameField).trim());

        if (className == CodeHelpers::makeValidIdentifier (className, false, true, false))
        {
            String code (BinaryData::jucer_InlineComponentTemplate_h);
            code = code.replace ("COMPONENTCLASS", className);

            insertTextAtCaret (code);
            break;
        }
    }
}
