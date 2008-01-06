/*
 *  SampleEditor.cpp
 *  milkytracker_universal
 *
 *  Created by Peter Barth on 22.11.07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "SampleEditor.h"
#include "SimpleVector.h"
#include "XModule.h"
#include "VRand.h"
#include "Equalizer.h"
#include "EQConstants.h"
#include "FilterParameters.h"
#include "SampleEditorResampler.h"

SampleEditor::ClipBoard* SampleEditor::ClipBoard::instance = NULL;

SampleEditor::ClipBoard* SampleEditor::ClipBoard::getInstance()
{
	if (instance == NULL)
		instance = new ClipBoard();
		
	return instance;
}

SampleEditor::ClipBoard::ClipBoard() :
		buffer(NULL)
{
}

SampleEditor::ClipBoard::~ClipBoard()
{
	delete[] buffer;
}
		
void SampleEditor::ClipBoard::makeCopy(TXMSample& sample, XModule& module, pp_int32 selectionStart, pp_int32 selectionEnd, bool cut/* = false*/)
{
	if (selectionEnd < 0)
		return;
	
	if (selectionStart < 0)
		selectionStart = 0;
		
	if (selectionEnd > (signed)sample.samplen)
		selectionEnd = sample.samplen;

	if (selectionEnd < selectionStart)
	{
		pp_int32 s = selectionEnd; selectionEnd = selectionStart; selectionStart = s;
	}
	
	this->selectionStart = selectionStart;
	this->selectionEnd = selectionEnd;
		
	this->selectionWidth = abs(selectionEnd - selectionStart); 
		
	if (selectionWidth == 0)
		return;

	if (buffer)
		delete[] buffer;	
		
	numBits = (sample.type & 16) ? 16 : 8;
	
	// 16 bit sample
	if (numBits == 16)
	{
		buffer = (mp_sbyte*)(new mp_sword[selectionWidth+1]);
		
		mp_sword* dstptr = (mp_sword*)buffer;
		for (pp_int32 i = selectionStart; i <= selectionEnd; i++)
			*dstptr++ = sample.getSampleValue(i);
	}
	// 8 bit sample
	else if (numBits == 8)
	{
		buffer = new mp_sbyte[selectionWidth+1];

		mp_sbyte* dstptr = (mp_sbyte*)buffer;
		for (pp_int32 i = selectionStart; i <= selectionEnd; i++)
			*dstptr++ = sample.getSampleValue(i);
	}
	else ASSERT(false);
}

void SampleEditor::ClipBoard::paste(TXMSample& sample, XModule& module, pp_int32 pos)
{
	if (pos < 0)
		pos = 0;

	if (sample.sample == NULL)
	{
		sample.samplen = 0;
		pos = 0;
	}

	pp_int32 newSampleSize = sample.samplen + selectionWidth;
	pp_int32 i;
	
	// 16 bit sample
	if (sample.type & 16)
	{
		mp_sword* newBuffer = (mp_sword*)module.allocSampleMem(newSampleSize*2);
		
		// copy stuff before insert start point
		for (i = 0;  i < pos; i++)
			sample.setSampleValue((mp_ubyte*)newBuffer, i, sample.getSampleValue(i));
		
		// copy selection to start point
		for (i = 0; i < selectionWidth; i++)
			sample.setSampleValue((mp_ubyte*)newBuffer, i+pos, getSampleWord(i));
			
		// copy stuff after insert start point
		for (i = 0;  i < ((signed)sample.samplen - pos); i++)
			sample.setSampleValue((mp_ubyte*)newBuffer, i+pos+selectionWidth, sample.getSampleValue(i+pos));
	
		if (sample.sample)
			module.freeSampleMem((mp_ubyte*)sample.sample);
		
		sample.sample = (mp_sbyte*)newBuffer;
	}
	else
	{
		mp_sbyte* newBuffer = (mp_sbyte*)module.allocSampleMem(newSampleSize);
		
		// copy stuff before insert start point
		for (i = 0;  i < pos; i++)
			sample.setSampleValue((mp_ubyte*)newBuffer, i, sample.getSampleValue(i));
		
		// copy selection to start point
		for (i = 0; i < selectionWidth; i++)
			sample.setSampleValue((mp_ubyte*)newBuffer, i+pos, getSampleByte(i));
			
		// copy stuff after insert start point
		for (i = 0;  i < ((signed)sample.samplen - pos); i++)
			sample.setSampleValue((mp_ubyte*)newBuffer, i+pos+selectionWidth, sample.getSampleValue(i+pos));

		if (sample.sample)
			module.freeSampleMem((mp_ubyte*)sample.sample);

		sample.sample = newBuffer;
	}

	pp_int32 loopend = sample.loopstart + sample.looplen;

	if ((signed)sample.loopstart < pos && loopend > pos)
		loopend+=selectionWidth;
	else if ((signed)sample.loopstart > pos && loopend > pos)
	{
		sample.loopstart+=selectionWidth;
		loopend+=selectionWidth;
	}

	sample.samplen = newSampleSize;

	sample.looplen = loopend - sample.loopstart;

}

void SampleEditor::prepareUndo()
{
	delete before; 
	before = NULL; 
		
	if (undoStackEnabled && undoStackActivated && undoStack) 
	{
		undoUserData.clear();
		notifyListener(NotificationFeedUndoData);

		before = new SampleUndoStackEntry(*sample, 
										  getSelectionStart(), 
										  getSelectionEnd(), 
										  &undoUserData);
	}
}

void SampleEditor::finishUndo()
{
	if (undoStackEnabled && undoStackActivated && undoStack) 
	{ 
		// first of all the listener should get the chance to adjust
		// user data according to our new changes BEFORE we actually save
		// the new state in the undo stack for redo
		lastOperationDidChangeSize = (sample->samplen != before->getSampLen());					
		notifyListener(NotificationChangesValidate);
		
		undoUserData.clear();
		// we want some user data now
		notifyListener(NotificationFeedUndoData);

		SampleUndoStackEntry after(SampleUndoStackEntry(*sample, 
										 getSelectionStart(), 
										 getSelectionEnd(), 
										 &undoUserData)); 
		if (*before != after) 
		{ 
			if (undoStack) 
			{ 
				undoStack->Push(*before); 
				undoStack->Push(after); 
				undoStack->Pop(); 
			} 
			// we're done, client might want to refresh the screen or whatever
			notifyListener(NotificationChanges);			
		} 
	} 
}
	
bool SampleEditor::revoke(const SampleUndoStackEntry* stackEntry)
{
	if (sample == NULL)
		return false;
	 if (undoStack == NULL || !undoStackEnabled)
		return false;
		
	sample->samplen = stackEntry->getSampLen();
	sample->loopstart = stackEntry->getLoopStart(); 
	sample->looplen = stackEntry->getLoopLen(); 
	sample->relnote = stackEntry->getRelNote(); 
	sample->finetune = stackEntry->getFineTune(); 
	sample->type = (mp_ubyte)stackEntry->getFlags();
	
	setSelectionStart(stackEntry->getSelectionStart());
	setSelectionEnd(stackEntry->getSelectionEnd());
	
	enterCriticalSection();
	
	// free old sample memory
	if (sample->sample)
	{
		module->freeSampleMem((mp_ubyte*)sample->sample);
		sample->sample = NULL;
	}
	
	if (stackEntry->getBuffer())
	{			
		if (sample->type & 16)
		{
			sample->sample = (mp_sbyte*)module->allocSampleMem(sample->samplen*2);
			TXMSample::copyPaddedMem(sample->sample, stackEntry->getBuffer(), sample->samplen*2);
		}
		else
		{
			sample->sample = (mp_sbyte*)module->allocSampleMem(sample->samplen);
			TXMSample::copyPaddedMem(sample->sample, stackEntry->getBuffer(), sample->samplen);
		}
	}
	
	leaveCriticalSection();
	undoUserData = stackEntry->getUserData();
	notifyListener(NotificationFetchUndoData);
	return true;
}

void SampleEditor::notifyChanges(bool condition, bool lazy/* = true*/)
{
	lastOperation = OperationRegular;	
	lastOperationDidChangeSize = false;
	if (!lazy)
	{
		setLazyUpdateNotifications(false);
		notifyListener(NotificationChangesValidate);
		notifyListener(NotificationChanges);
	}
	else
	{
		setLazyUpdateNotifications(true);
		notifyListener(NotificationChanges);
		setLazyUpdateNotifications(false);
	}
}

SampleEditor::SampleEditor() :
	EditorBase(),
	sample(NULL),
	undoStackEnabled(true), 
	undoStackActivated(true),	
	before(NULL),
	undoStack(NULL),
	lastOperationDidChangeSize(false),
	lastOperation(OperationRegular),
	drawing(false),
	lastSamplePos(-1),
	lastParameters(NULL),
	lastFilterFunc(NULL)
{
	// Undo history
	undoHistory = new UndoHistory<TXMSample, SampleUndoStackEntry>(UNDOHISTORYSIZE_SAMPLEEDITOR);
	
	resetSelection();

	memset(&lastSample, 0, sizeof(lastSample));
}

SampleEditor::~SampleEditor()
{
	delete lastParameters;
	delete undoHistory;
	delete undoStack;
	delete before;
}

void SampleEditor::attachSample(TXMSample* sample, XModule* module) 
{
	// only return if the sample data really equals what we already have
	if (sample->equals(lastSample) && sample == this->sample)
		return;

	lastSample = *sample;

	// --------- update undo history information --------------------	
	if (undoStackEnabled && undoStackActivated)
	{
		if (undoStack)
		{	
			// if the undo stack is empty, we don't need to save current undo stack
			if (!undoStack->IsEmpty() || !undoStack->IsTop())
			{	
				undoStack = undoHistory->getUndoStack(sample, this->sample, undoStack);
			}
			// delete it if it's empty
			else
			{
				delete undoStack;
				undoStack = NULL;
				
				undoStack = undoHistory->getUndoStack(sample, NULL, NULL);
			}
		}
		
		// couldn't get any from history, create new one
		if (!undoStack)
		{
			undoStack = new PPUndoStack<SampleUndoStackEntry>(UNDODEPTH_SAMPLEEDITOR);
		}
	}

	this->sample = sample;
	attachModule(module);

	resetSelection();
	
	notifyListener(NotificationReload);
}

void SampleEditor::reset()
{
	if (undoStackEnabled)
	{
		if (undoHistory)
			delete undoHistory;
		
		if (undoStack)
		{
			delete undoStack;
			undoStack = NULL;
			undoStack = new PPUndoStack<SampleUndoStackEntry>(UNDODEPTH_SAMPLEEDITOR);
		}
		
		undoHistory = new UndoHistory<TXMSample, SampleUndoStackEntry>(UNDOHISTORYSIZE_SAMPLEEDITOR);
	}
	else
	{
		if (undoHistory)
		{
			delete undoHistory;
			undoHistory = NULL;	
		}
		
		if (undoStack)
		{
			delete undoStack;
			undoStack = NULL;
		}
	}
}

bool SampleEditor::isEmptySample() const  
{
	if (!isValidSample())
		return true;
	
	return (sample->sample == NULL);
}

bool SampleEditor::canMinimize() const
{
	if (!isValidSample())
		return false;
	
	return sample->samplen && sample->sample && (sample->type & 3);
}

bool SampleEditor::isEditableSample() const
{
	if (!isValidSample())
		return false;
	
	return (sample->sample != NULL) && (sample->samplen != 0);
}

void SampleEditor::enableUndoStack(bool enable)
{
	undoStackEnabled = enable;
	reset();
}

bool SampleEditor::undo()
{
	if (!undoStackEnabled || undoStack == NULL) return false;
	if (undoStack->IsEmpty()) return false;
	return revoke(undoStack->Pop());
}

bool SampleEditor::redo()
{
	if (!undoStackEnabled || undoStack == NULL) return false;
	if (undoStack->IsTop()) return false;
	return revoke(undoStack->Advance());
}

void SampleEditor::selectAll()
{
	if (isEmptySample())
		return;

	selectionStart = 0;
	selectionEnd = sample->samplen;
}

bool SampleEditor::validate()
{
	if (isEmptySample())
	{
		resetSelection();
		return false;
	}

	pp_int32 sStart = getSelectionStart();
	pp_int32 sEnd = getSelectionEnd();
	if (sEnd < sStart)
	{
		pp_int32 s = sEnd; sEnd = sStart; sStart = s;
	}
	setSelectionStart(sStart);
	setSelectionEnd(sEnd);

	if (getSelectionEnd() != -1 && getSelectionStart () != -1 &&
		getSelectionEnd() < 0)
	{
		resetSelection();
	}

	if (getSelectionEnd() > 0 && getSelectionStart() < 0)
		setSelectionStart(0);

	if (getSelectionStart() > (signed)sample->samplen)
	{
		resetSelection();
	}
	if (getSelectionEnd() > (signed)sample->samplen)
	{
		setSelectionEnd(sample->samplen);
	}
	
	if (sample->loopstart > sample->samplen)
		sample->loopstart = 0;
	if (sample->loopstart + sample->looplen > sample->samplen)
		sample->looplen -= (sample->loopstart + sample->looplen) - sample->samplen;
	
	// one shot sample only allows loopstart == 0
	if ((sample->type & 32) && sample->loopstart)
	{
		sample->type &= ~32;
	}
	return true;
}

bool SampleEditor::canPaste() const
{ 
	if (selectionEnd == selectionStart &&
		selectionStart == -1 &&
		sample->sample != NULL)
		return false;

	return !ClipBoard::getInstance()->isEmpty(); 
}

pp_uint32 SampleEditor::getRepeatStart() const
{
	return sample == NULL ? 0 : sample->loopstart;
}

pp_uint32 SampleEditor::getRepeatEnd() const
{
	return sample == NULL ? 0 : sample->loopstart + sample->looplen;
}

pp_uint32 SampleEditor::getRepeatLength() const
{
	return sample == NULL ? 0 : sample->looplen;
}

void SampleEditor::setRepeatStart(pp_uint32 start)
{
	if (sample == NULL)
		return;

	mp_uint32 before = sample->loopstart;		
		
	sample->loopstart = start;

	validate();

	notifyChanges(before != sample->loopstart, false);
}

void SampleEditor::setRepeatEnd(pp_uint32 end)
{
	if (sample == NULL)
		return;
		
	mp_uint32 before = sample->looplen;	
		
	sample->looplen = (end - sample->loopstart);
	
	validate();

	notifyChanges(before != sample->looplen, false);
}

void SampleEditor::setRepeatLength(pp_uint32 length)
{
	if (sample == NULL)
		return;

	mp_uint32 before = sample->looplen;	
		
	sample->looplen = length;	

	validate();

	notifyChanges(before != sample->looplen, false);
}

bool SampleEditor::increaseRepeatStart()
{
	if (isEmptySample())
		return false;
	
	mp_uint32 before = sample->loopstart;
	
	pp_int32 loopend = sample->loopstart+sample->looplen;
	pp_int32 loopstart = sample->loopstart+1;
	if (loopstart >= 0 && loopstart < loopend && loopend >= 0 && loopend <= (signed)sample->samplen)
	{
		sample->looplen = loopend - loopstart;
		sample->loopstart = loopstart;
	}
	
	validate();

	notifyChanges(before != sample->loopstart, false);

	return true;
}

bool SampleEditor::decreaseRepeatStart()
{
	if (isEmptySample())
		return false;

	mp_uint32 before = sample->loopstart;

	pp_int32 loopend = sample->loopstart+sample->looplen;
	pp_int32 loopstart = sample->loopstart-1;
	if (loopstart >= 0 && loopstart < loopend && loopend >= 0 && loopend <= (signed)sample->samplen)
	{
		sample->looplen = loopend - loopstart;
		sample->loopstart = loopstart;
	}

	validate();

	notifyChanges(before != sample->loopstart, false);

	return true;
}

bool SampleEditor::increaseRepeatLength()
{
	if (isEmptySample())
		return false;

	mp_uint32 before = sample->looplen;
	
	pp_int32 loopend = sample->loopstart+sample->looplen+1;
	pp_int32 loopstart = sample->loopstart;
	if (loopstart >= 0 && loopstart < loopend && loopend >= 0 && loopend <= (signed)sample->samplen)
	{
		sample->looplen = loopend - loopstart;
		sample->loopstart = loopstart;
	}

	validate();

	notifyChanges(before != sample->looplen, false);

	return true;
}

bool SampleEditor::decreaseRepeatLength()
{
	if (isEmptySample())
		return false;

	mp_uint32 before = sample->looplen;

	pp_int32 loopend = sample->loopstart+sample->looplen-1;
	pp_int32 loopstart = sample->loopstart;
	if (loopstart >= 0 && loopstart < loopend && loopend >= 0 && loopend <= (signed)sample->samplen)
	{
		sample->looplen = loopend - loopstart;
		sample->loopstart = loopstart;
	}
	
	validate();
	
	notifyChanges(before != sample->looplen, false);

	return true;
}

bool SampleEditor::setLoopType(pp_uint8 type)
{
	if (sample == NULL)
		return false;

	mp_ubyte before = sample->type;

	if (type <= 2)
	{
		sample->type &= ~(3+32);
		sample->type |= type;
		
		if (type && 
			sample->loopstart == 0 && 
			sample->looplen == 0)
		{
			sample->loopstart = 0;
			sample->looplen = sample->samplen;
		}
	}
	else if (type == 3)
	{
		sample->type &= ~(3+32);
		sample->type |= (1+32);
		mp_sint32 loopend = sample->loopstart + sample->looplen;
		sample->loopstart = 0;
		sample->looplen = loopend;
	}
	else ASSERT(false);
	
	notifyChanges(before != sample->type);

	return true;
}

pp_uint8 SampleEditor::getLoopType() const
{ 
	if (sample) 
	{
		if ((sample->type & 3) == 1 && (sample->type & 32))
			return 3;
		else
			return sample->type & 3;
	}
	else 
		return 0; 
}

bool SampleEditor::is16Bit() const
{ 
	if (sample) 
		return (sample->type & 16) == 16;
	else 
		return false; 
}

pp_int32 SampleEditor::getRelNoteNum() const
{
	return sample ? sample->relnote : 0;
}

void SampleEditor::increaseRelNoteNum(pp_int32 offset)
{
	if (sample == NULL)
		return;
		
	mp_sbyte before = sample->relnote;
		
	pp_int32 relnote = sample->relnote;
	relnote+=offset;
	if (relnote > 71)
		relnote = 71;
	if (relnote < -48)
		relnote = -48;
	sample->relnote = (mp_sbyte)relnote;

	notifyChanges(sample->relnote != before);
}

pp_int32 SampleEditor::getFinetune() const
{
	return sample ? sample->finetune : 0;
}

void SampleEditor::setFinetune(pp_int32 finetune)
{
	if (sample == NULL)
		return;

	mp_sbyte before = sample->finetune;

	if (finetune < -128)
		finetune = -128;
	if (finetune > 127)
		finetune = 127; 

	sample->finetune = (mp_sbyte)finetune;

	notifyChanges(sample->finetune != before);
}

void SampleEditor::setFT2Volume(pp_int32 vol)
{
	if (sample == NULL)
		return;

	mp_ubyte before = sample->vol;

	sample->vol = XModule::vol64to255(vol);
	
	notifyChanges(sample->vol != before);
}

pp_int32 SampleEditor::getFT2Volume() const
{
	return sample ? XModule::vol255to64(sample->vol) : 0;
}

void SampleEditor::setPanning(pp_int32 pan)
{
	if (sample == NULL)
		return;

	mp_sbyte before = sample->pan;

	if (pan < 0) pan = 0;
	if (pan > 255) pan = 255;
	sample->pan = (mp_sbyte)pan;
	
	notifyChanges(sample->pan != before);
}

pp_int32 SampleEditor::getPanning() const
{
	return sample ? sample->pan : 0;
}

void SampleEditor::startDrawing()
{
	drawing = true;
	lastSamplePos = -1;
	prepareUndo();
}

void SampleEditor::drawSample(pp_int32 sampleIndex, float s)
{
	s*=2.0f;

	pp_int32 from = lastSamplePos;
	pp_int32 to = sampleIndex;
	if (from == -1)
		from = sampleIndex;

	float froms = 0.0f;
	froms = getFloatSampleFromWaveform(from);

	if (from > to)
	{
		pp_int32 h = from; from = to; to = h;
		float fh = froms; froms = s; s = fh;
	}
	
	float step = 0;
	if (to-from)
		step = (s-froms)/(to-from);
	else
		froms = s;
	
	lastSamplePos = sampleIndex;

	for (pp_int32 si = from; si <= to; si++)
	{
		setFloatSampleInWaveform(si, froms);
		froms+=step;
	}
	
}

void SampleEditor::endDrawing()
{
	drawing = false;
	lastSamplePos = -1;
	if (!sample || !sample->sample || !sample->samplen)
		return;
	
	lastOperation = OperationRegular;
	finishUndo();
}

bool SampleEditor::cutSampleInternal()
{
	if (sample == NULL)
		return false;

	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	if (sStart >= 0 && sEnd >= 0)
	{		
		if (sEnd < sStart)
		{
			pp_int32 s = sEnd; sEnd = sStart; sStart = s;
		}
	}
	else return false;
	
	selectionStart = sStart;
	selectionEnd = sEnd;

	if (sStart == sEnd)
		return false;

	if (sEnd < (signed)sample->loopstart + (signed)sample->looplen)
		sample->loopstart-=(sEnd - sStart);
	if ((signed)sample->loopstart < 0)
		sample->loopstart = 0;

	if (sample->type & 16)
	{
		mp_sword* buff = (mp_sword*)sample->sample;
		for (pp_uint32 i = selectionEnd; i <= sample->samplen; i++)
			buff[(i-selectionEnd)+selectionStart] = buff[i];
	}
	else
	{
		mp_sbyte* buff = (mp_sbyte*)sample->sample;
		for (pp_uint32 i = selectionEnd; i <= sample->samplen; i++)
			buff[(i-selectionEnd)+selectionStart] = buff[i];
	}
	
	sample->samplen -= abs(selectionEnd - selectionStart);
	
	return true;
}

void SampleEditor::cut()
{
	if (sample == NULL)
		return;

	if (!hasValidSelection())
		return;

	// we're going to change the sample buffers, better stop
	enterCriticalSection();
	// undo stuff going on
	prepareUndo();

	// store selection into clipboard
	ClipBoard::getInstance()->makeCopy(*sample, *module, getSelectionStart(), getSelectionEnd());

	// just make clear what kind of an operation this is
	if (cutSampleInternal())
		lastOperation = OperationCut;
	
	// selection no longer intact
	resetSelection();
	
	// validate our internal state
	validate();	
	// redo stuff and client notifications
	finishUndo();
	// keep on playing if you did
	leaveCriticalSection();
}

void SampleEditor::copy()
{
	if (sample == NULL)
		return;

	if (!hasValidSelection())
		return;

	ClipBoard::getInstance()->makeCopy(*sample, *module, getSelectionStart(), getSelectionEnd());

	notifyListener(NotificationUpdateNoChanges);
}

void SampleEditor::paste()
{
	if (sample == NULL)
		return;

	enterCriticalSection();

	prepareUndo();

	if (hasValidSelection())
	{
		mp_uint32 loopstart = sample->loopstart;
		mp_uint32 looplen = sample->looplen;
		if (cutSampleInternal())
			lastOperation = OperationCut;
		sample->loopstart = loopstart;
		sample->looplen = looplen;
	}

	ClipBoard::getInstance()->paste(*sample, *module, getSelectionStart());

	setSelectionEnd(getSelectionStart() + ClipBoard::getInstance()->getWidth());

	validate();	
	finishUndo();

	leaveCriticalSection();
}

SampleEditor::WorkSample* SampleEditor::createWorkSample(pp_uint32 size, pp_uint8 numBits, pp_uint32 sampleRate)
{
	WorkSample* workSample = new WorkSample(*module, size, numBits, sampleRate);
	if (workSample->buffer == NULL)
	{
		delete workSample;
		return NULL;
	}
	
	return workSample;
}

void SampleEditor::pasteOther(WorkSample& src)
{
	enterCriticalSection();

	prepareUndo();

	if (sample->sample)
	{
		module->freeSampleMem((mp_ubyte*)sample->sample);
		sample->sample = NULL;
		sample->samplen = 0;
	}
	
	sample->loopstart = 0;
	sample->looplen = 0;
	sample->type = (src.numBits == 16) ? 16 : 0;
	sample->samplen = src.size;

	mp_sbyte rn, ft;
	XModule::convertc4spd((mp_uint32)src.sampleRate, &ft, &rn);
	sample->relnote = rn;
	sample->finetune = ft;
	
	sample->sample = (mp_sbyte*)src.buffer;
	src.buffer = NULL;
	
	finishUndo();
	
	leaveCriticalSection();
}

static float ppfabs(float f)
{
	return f < 0 ? -f : f;
}

float SampleEditor::getFloatSampleFromWaveform(pp_int32 index, void* src/* = NULL*/, pp_int32 size/* = 0*/)
{
	if (isEmptySample())
		return 0.0f;
		
	if (!src)
	{
		if (index > (signed)sample->samplen)
			index = sample->samplen;
		if (index < 0)
			index = 0;
	}
	else if (size != 0)
	{
		if (index >= size)
			index = size-1;
		if (index < 0)
			index = 0;
	}
			
	if (sample->type & 16)
	{
		mp_sword s = src ? *(((mp_sword*)src)+index) : sample->getSampleValue(index);
		return s > 0 ? (float)s*(1.0f/32767.0f) : (float)s*(1.0f/32768.0f);
	}
	else
	{
		mp_sbyte s = src ? *(((mp_sbyte*)src)+index) : sample->getSampleValue(index);
		return s > 0 ? (float)s*(1.0f/127.0f) : (float)s*(1.0f/128.0f);
	}
}

void SampleEditor::setFloatSampleInWaveform(pp_int32 index, float singleSample, void* src/* = NULL*/)
{
	if (isEmptySample() || index > (signed)sample->samplen)
		return;
	
	if (index < 0)
		index = 0;
				
	if (singleSample > 1.0f)
		singleSample = 1.0f;
	if (singleSample < -1.0f)
		singleSample = -1.0f;

	if (sample->type & 16)
	{
		mp_sword s = singleSample > 0 ? (mp_sword)(singleSample*32767.0f+0.5f) : (mp_sword)(singleSample*32768.0f-0.5f);
		if (src)
			*(((mp_sword*)src)+index) = s;
		else
			sample->setSampleValue(index, s);
	}
	else
	{
		mp_sbyte s = singleSample > 0 ? (mp_sbyte)(singleSample*127.0f+0.5f) : (mp_sbyte)(singleSample*128.0f-0.5f);
		if (src)
			*(((mp_sbyte*)src)+index) = s;
		else
			sample->setSampleValue(index, s);
	}
}

void SampleEditor::preFilter(TFilterFunc filterFuncPtr, const FilterParameters* par)
{
	if (filterFuncPtr)
	{
		if (par != NULL)
		{
			FilterParameters newPar(*par);
			if (lastParameters)
			{
				delete lastParameters;
				lastParameters = NULL;
			}
			lastParameters = new FilterParameters(newPar);			
		}
		else
		{
			if (lastParameters)
			{
				delete lastParameters;
				lastParameters = NULL;
			}
		}
		
		lastFilterFunc = filterFuncPtr;
	}

	enterCriticalSection();
	
	lastOperation = OperationRegular;

	notifyListener(NotificationPrepareLengthy);
}

void SampleEditor::postFilter()
{
	notifyListener(NotificationUnprepareLengthy);

	leaveCriticalSection();
}

bool SampleEditor::tool_newSample(pp_int32 numSamples, pp_int32 numBits)
{
	if (!isValidSample())
		return false;

	preFilter(NULL, NULL);
	
	prepareUndo();

	if (sample->sample)
	{
		module->freeSampleMem((mp_ubyte*)sample->sample);
		sample->sample = NULL;
	}
	
	sample->samplen = numSamples;
	sample->loopstart = 0;
	sample->looplen = sample->samplen;
	
	switch (numBits)
	{
		case 8:
			sample->sample = (mp_sbyte*)module->allocSampleMem(sample->samplen);
			memset(sample->sample, 0, sample->samplen);
			break;
		case 16:
			sample->type |= 16;
			sample->sample = (mp_sbyte*)module->allocSampleMem(sample->samplen*2);
			memset(sample->sample, 0, sample->samplen*2);
			break;
		default:
			ASSERT(false);
	}
	
	finishUndo();

	lastOperation = OperationNew;
	postFilter();
	
	if (sample->sample == NULL)
		return false;
		
	return true;
}

void SampleEditor::tool_minimizeSample()
{
	if (isEmptySample())
		return;

	if (!(sample->type & 3))
		return;

	preFilter(NULL, NULL);

	prepareUndo();

	pp_int32 loopend = sample->loopstart+sample->looplen;
	
	if (loopend > (signed)sample->samplen)
		loopend = sample->samplen;
	
	sample->samplen = loopend;

	finishUndo();
	
	postFilter();
}

void SampleEditor::tool_cropSample()
{
	if (isEmptySample())
		return;

	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	if (sStart >= 0 && sEnd >= 0)
	{		
		if (sEnd < sStart)
		{
			pp_int32 s = sEnd; sEnd = sStart; sStart = s;
		}
	}
	else return;
	
	selectionStart = sStart;
	selectionEnd = sEnd;

	if (sStart == sEnd)
		return;
		
	preFilter(NULL, NULL);
	
	prepareUndo();
	
	if (sample->type & 16)
	{
		mp_sword* buff = (mp_sword*)sample->sample;
		for (pp_int32 i = selectionStart; i < selectionEnd; i++)
			buff[i-selectionStart] = buff[i];
	}
	else
	{
		mp_sbyte* buff = (mp_sbyte*)sample->sample;
		for (pp_int32 i = selectionStart; i < selectionEnd; i++)
			buff[i-selectionStart] = buff[i];
	}
	
	sample->samplen = abs(selectionEnd - selectionStart);
	
	if (sample->loopstart > sample->samplen)
		sample->loopstart = 0;
	
	pp_int32 loopend = sample->loopstart + sample->looplen;
	
	if (loopend > (signed)sample->samplen)
		loopend = sample->samplen;
	
	sample->looplen = loopend - sample->loopstart;
	
	selectionStart = 0;
	selectionEnd = sample->samplen;
	
	finishUndo();	
	
	postFilter();
}

void SampleEditor::tool_clearSample()
{
	preFilter(NULL, NULL);
	
	prepareUndo();

	module->freeSampleMem((mp_ubyte*)sample->sample);
	sample->sample = NULL;
	sample->samplen = 0;
	sample->loopstart = 0;
	sample->looplen = 0;
	
	finishUndo();

	postFilter();
}

void SampleEditor::tool_convertSampleResolution(bool convert)
{
	preFilter(NULL, NULL);

	prepareUndo();

	if (sample->type & 16)
	{
		
		if (!convert)
		{
			sample->type &= ~16;
			sample->samplen<<=1;
			sample->looplen<<=1;
			sample->loopstart<<=1;	

		}
		else
		{
			mp_sbyte* buffer = new mp_sbyte[sample->samplen];
			
			for (mp_sint32 i = 0; i < (signed)sample->samplen; i++)
				buffer[i] = (mp_sbyte)(sample->getSampleValue(i)>>8);
			
			module->freeSampleMem((mp_ubyte*)sample->sample);
			sample->type &= ~16;
			sample->sample = (mp_sbyte*)module->allocSampleMem(sample->samplen);
			memcpy(sample->sample, buffer, sample->samplen);
			
			delete[] buffer;
		}
	}
	else
	{
		if (!convert)
		{
			sample->type |= 16;
			sample->samplen>>=1;
			sample->looplen>>=1;
			sample->loopstart>>=1;
		}
		else
		{			
			mp_sword* buff16 = new mp_sword[sample->samplen];
			
			for (mp_sint32 i = 0; i < (signed)sample->samplen; i++)
				buff16[i] = (mp_sword)(sample->getSampleValue(i)<<8);
			
			module->freeSampleMem((mp_ubyte*)sample->sample);
			sample->type |= 16;
			sample->sample = (mp_sbyte*)module->allocSampleMem(sample->samplen*2);
			memcpy(sample->sample, buff16, sample->samplen*2);
			
			delete[] buff16;
		}
	}

	finishUndo();
	
	postFilter();
}

void SampleEditor::tool_mixPasteSample()
{
	if (isEmptySample())
		return;

	if (ClipBoard::getInstance()->isEmpty())
		return;

	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(NULL, NULL);
	
	prepareUndo();
	
	ClipBoard* clipBoard = ClipBoard::getInstance();
	
	float step = (float)clipBoard->getWidth() / (float)(sEnd-sStart);
	
	float j = 0.0f;
	for (pp_int32 i = sStart; i < sEnd; i++)
	{
		float frac = j - (float)floor(j);
	
		pp_int16 s = clipBoard->getSampleWord((pp_int32)j);
		float f1 = s < 0 ? (s/32768.0f) : (s/32767.0f);
		s = clipBoard->getSampleWord((pp_int32)j+1);
		float f2 = s < 0 ? (s/32768.0f) : (s/32767.0f);

		float f = (1.0f-frac)*f1 + frac*f2;
		
		setFloatSampleInWaveform(i, f + getFloatSampleFromWaveform(i));
		j+=step;
	}
				
	finishUndo();	
	
	postFilter();

}

void SampleEditor::tool_scaleSample(const FilterParameters* par)
{
	if (isEmptySample())
		return;

	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_scaleSample, par);
	
	prepareUndo();
	
	float startScale = par->getParameter(0);
	float endScale = par->getParameter(1);
	
	float step = (endScale - startScale) / (float)(sEnd - sStart);
	
	for (pp_int32 i = sStart; i < sEnd; i++)
	{
		float f = getFloatSampleFromWaveform(i);
		setFloatSampleInWaveform(i, f*startScale);
		startScale+=step;
	}
				
	finishUndo();	
	
	postFilter();
}

void SampleEditor::tool_normalizeSample(const FilterParameters* par)
{
	if (isEmptySample())
		return;

	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_normalizeSample, par);
	
	prepareUndo();
	
	float maxLevel = ((par == NULL)? 1.0f : par->getParameter(0));
	float peak = 0.0f;

	pp_int32 i;

	// find peak value
	for (i = sStart; i < sEnd; i++)
	{
		float f = getFloatSampleFromWaveform(i);
		if (ppfabs(f) > peak) peak = ppfabs(f);
	}
	
	float scale = maxLevel / peak;
	
	for (i = sStart; i < sEnd; i++)
	{
		float f = getFloatSampleFromWaveform(i);
		setFloatSampleInWaveform(i, f*scale);
	}
				
	finishUndo();	
	
	postFilter();
}

void SampleEditor::tool_reverseSample(const FilterParameters* par)
{
	if (isEmptySample())
		return;

	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_reverseSample, par);
	
	prepareUndo();
	
	pp_int32 i;
	for (i = 0; i < (sEnd-sStart)>>1; i++)
	{
		float f1 = getFloatSampleFromWaveform(sStart + i);
		float f2 = getFloatSampleFromWaveform(sEnd - 1 - i);
		float h = f2;
		f2 = f1; f1 = h;
		setFloatSampleInWaveform(sStart + i, f1);
		setFloatSampleInWaveform(sEnd - i, f2);
	}
				
	finishUndo();	
	
	postFilter();
}

void SampleEditor::tool_PTboostSample(const FilterParameters* par)
{
	if (isEmptySample())
		return;

	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_PTboostSample, par);
	
	prepareUndo();
	
	pp_int32 i;
	
	float d0 = 0.0f, d1, d2;
	for (i = sStart; i < sEnd; i++)
	{
		d1 = d2 = getFloatSampleFromWaveform(i);
		d1 -= d0;
		d0 = d2;
		
		if (d1 < 0.0f)
		{
			d1 = -d1;
			d1*= 0.25f;
			d2 -= d1;
		}
		else
		{
			d1*= 0.25f;
			d2 += d1;
		}
		
		if (d2 > 1.0f)
			d2 = 1.0f;
		
		if (d2 < -1.0f)
			d2 = -1.0f;
		
		setFloatSampleInWaveform(i, d2);
	}
	
	finishUndo();	
	
	postFilter();
}

bool SampleEditor::isValidxFadeSelection()
{
	if (isEmptySample() || !hasValidSelection() || !(sample->type & 3))
		return false;

	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (sStart >= 0 && sEnd >= 0)
	{		
		if (sEnd < sStart)
		{
			pp_int32 s = sEnd; sEnd = sStart; sStart = s;
		}
	}
	
	pp_uint32 loopend = sample->loopstart + sample->looplen;
		
	if (((unsigned)sStart <= sample->loopstart && (unsigned)sEnd >= loopend) ||
		((unsigned)sStart > sample->loopstart && (unsigned)sEnd < loopend) ||
		((unsigned)sStart < sample->loopstart && (unsigned)sEnd < sample->loopstart) || 
		((unsigned)sStart > loopend && (unsigned)sEnd > loopend))
		return false;
		
	return true;
}

void SampleEditor::tool_xFadeSample(const FilterParameters* par)
{
	if (!isValidxFadeSelection())
		return;

	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (sStart >= 0 && sEnd >= 0)
	{		
		if (sEnd < sStart)
		{
			pp_int32 s = sEnd; sEnd = sStart; sStart = s;
		}
	}
	
	if (!(sample->type & 3) || sEnd < (signed)sample->loopstart || sStart > (signed)(sample->loopstart + sample->looplen))
		return;

	pp_int32 loopend = sample->loopstart + sample->looplen;
		
	preFilter(&SampleEditor::tool_xFadeSample, par);
	
	if (sStart <= (signed)sample->loopstart && sEnd >= loopend)
		return;
		
	if (sStart >= (signed)sample->loopstart && sEnd >= loopend)
	{
		sStart-=loopend;
		sStart+=sample->loopstart;
		sEnd-=loopend;
		sEnd+=sample->loopstart;
	}
	
	mp_ubyte* buffer = new mp_ubyte[(sample->type & 16) ? sample->samplen*2 : sample->samplen];
	if (!buffer)
		return;

	memcpy(buffer, sample->sample, (sample->type & 16) ? sample->samplen*2 : sample->samplen);

	prepareUndo();	
	
	pp_int32 i = 0;
	
	// loop start
	if ((sample->type & 3) == 1)
	{	
		for (i = sStart; i < (signed)sample->loopstart; i++)
		{
			float t = (((float)i - sStart) / (float)(sample->loopstart - sStart))*0.5f;
			
			float f1 = getFloatSampleFromWaveform(i, buffer, sample->samplen);
			float f2 = getFloatSampleFromWaveform(loopend - (sample->loopstart - sStart) + (i - sStart), buffer, sample->samplen);		
			
			float f = f1*(1.0f-t) + f2*t;
			setFloatSampleInWaveform(i, f);
		}
		
		for (i = sample->loopstart; i < sEnd; i++)
		{
			float t = 0.5f - ((((float)i - sample->loopstart) / (float)(sEnd-sample->loopstart))*0.5f);
			
			float f1 = getFloatSampleFromWaveform(i, buffer, sample->samplen);
			float f2 = getFloatSampleFromWaveform(loopend + (i - sample->loopstart), buffer, sample->samplen);		
			
			float f = f1*(1.0f-t) + f2*t;
			setFloatSampleInWaveform(i, f);
		}
		
		// loop end
		sStart-=sample->loopstart;
		sStart+=loopend;
		sEnd-=sample->loopstart;
		sEnd+=loopend;	
		
		for (i = sStart; i < loopend; i++)
		{
			float t = (((float)i - sStart) / (float)(loopend - sStart))*0.5f;
			
			float f1 = getFloatSampleFromWaveform(i, buffer, sample->samplen);
			float f2 = getFloatSampleFromWaveform(sample->loopstart - (loopend - sStart) + (i - sStart), buffer, sample->samplen);		
			
			float f = f1*(1.0f-t) + f2*t;
			setFloatSampleInWaveform(i, f);
		}	
		
		for (i = loopend; i < sEnd; i++)
		{
			float t = 0.5f - ((((float)i - loopend) / (float)(sEnd-loopend))*0.5f);
			
			float f1 = getFloatSampleFromWaveform(i, buffer, sample->samplen);
			float f2 = getFloatSampleFromWaveform(sample->loopstart + (i - loopend), buffer, sample->samplen);		
			
			float f = f1*(1.0f-t) + f2*t;
			setFloatSampleInWaveform(i, f);
		}
		
	}
	else if ((sample->type & 3) == 2)
	{
		for (i = sStart; i < (signed)sample->loopstart; i++)
		{
			float t = (((float)i - sStart) / (float)(sample->loopstart - sStart))*0.5f;
			
			float f1 = getFloatSampleFromWaveform(i, buffer, sample->samplen);
			float f2 = getFloatSampleFromWaveform(sample->loopstart + (i - sStart), buffer, sample->samplen);		
			
			float f = f1*(1.0f-t) + f2*t;
			setFloatSampleInWaveform(i, f);
		}
		
		for (i = sample->loopstart; i < sEnd; i++)
		{
			float t = 0.5f - ((((float)i - sample->loopstart) / (float)(sEnd-sample->loopstart))*0.5f);
			
			float f1 = getFloatSampleFromWaveform(i, buffer, sample->samplen);
			float f2 = getFloatSampleFromWaveform(sample->loopstart - (i - sample->loopstart), buffer, sample->samplen);		
			
			float f = f1*(1.0f-t) + f2*t;
			setFloatSampleInWaveform(i, f);
		}
	}
	
	delete[] buffer;
	
	finishUndo();	
	
	postFilter();
}

void SampleEditor::tool_changeSignSample(const FilterParameters* par)
{
	if (isEmptySample())
		return;

	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_changeSignSample, par);
	
	prepareUndo();
	
	pp_int32 i;
	// lazyness follows
	for (i = sStart; i < sEnd; i++)
	{
		if (sample->type & 16)
		{
			mp_uword* smp = (mp_uword*)sample->sample;		
			smp[i]^=0x7fff;
		}
		else
		{
			mp_ubyte* smp = (mp_ubyte*)sample->sample;		
			smp[i]^=0x7f;
		}
	}
	
	finishUndo();	
	
	postFilter();
}

void SampleEditor::tool_swapByteOrderSample(const FilterParameters* par)
{
	if (isEmptySample())
		return;
		
	if (!(sample->type & 16))
		return;

	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_swapByteOrderSample, par);
	
	prepareUndo();
	
	pp_int32 i;

	mp_uword* smp = (mp_uword*)sample->sample;		
	for (i = sStart; i < sEnd; i++)
	{
		mp_uword s = (smp[i] >> 8) | ((smp[i] & 0xFF) << 8);
		smp[i] = s;
	}
	
	finishUndo();	
	
	postFilter();
}

float getc4spd(mp_sint32 relnote,mp_sint32 finetune);

void SampleEditor::tool_resampleSample(const FilterParameters* par)
{
	if (isEmptySample())
		return;
		
	preFilter(&SampleEditor::tool_resampleSample, par);
	
	prepareUndo();

	float c4spd = getc4spd(sample->relnote, sample->finetune);

	float step = c4spd / par->getParameter(0);

	pp_int32 resamplerType = (pp_int32)par->getParameter(1);

	SampleEditorResampler resampler(*module, *sample, (SampleEditorResampler::ResamplerTypes)resamplerType);
	
	bool res = resampler.resample(step);
	
	if (res)
	{
		pp_uint32 c4spdi = (mp_uint32)par->getParameter(0);
		mp_sbyte rn, ft;
		XModule::convertc4spd((mp_uint32)c4spdi, &ft, &rn);
		sample->relnote = rn;
		sample->finetune = ft;
	}
	
	lastOperation = OperationCut;
	finishUndo();	
	
	postFilter();
}

void SampleEditor::tool_DCNormalizeSample(const FilterParameters* par)
{
	if (isEmptySample())
		return;
		
	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_DCNormalizeSample, par);
	
	prepareUndo();
	
	pp_int32 i;

	float DC = 0.0f;
	for (i = sStart; i < sEnd; i++)
	{
		DC += getFloatSampleFromWaveform(i);		
	}
	DC = DC / (float)(sEnd-sStart);
	for (i = sStart; i < sEnd; i++)
	{
		setFloatSampleInWaveform(i, getFloatSampleFromWaveform(i) - DC);
	}
	
	finishUndo();	
	
	postFilter();
}

void SampleEditor::tool_DCOffsetSample(const FilterParameters* par)
{
	if (isEmptySample())
		return;
		
	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_DCOffsetSample, par);
	
	prepareUndo();
	
	pp_int32 i;

	float DC = par->getParameter(0);
	for (i = sStart; i < sEnd; i++)
	{
		setFloatSampleInWaveform(i, getFloatSampleFromWaveform(i) + DC);
	}
	
	finishUndo();	
	
	postFilter();
}

void SampleEditor::tool_rectangularSmoothSample(const FilterParameters* par)
{
	if (isEmptySample())
		return;
		
	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_rectangularSmoothSample, par);
	
	mp_sint32 sLen = sEnd - sStart;
	
	mp_ubyte* buffer = new mp_ubyte[(sample->type & 16) ? sLen*2 : sLen];
	if (!buffer)
		return;

	memcpy(buffer, sample->sample + sStart, (sample->type & 16) ? sLen*2 : sLen);

	prepareUndo();	
	
	pp_int32 i;

	for (i = sStart; i < sEnd; i++)
	{
		float f = (getFloatSampleFromWaveform(i - sStart - 1, buffer, sLen) +
				  getFloatSampleFromWaveform(i - sStart, buffer, sLen) +
				  getFloatSampleFromWaveform(i - sStart + 1, buffer, sLen)) * (1.0f/3.0f);
				  
		setFloatSampleInWaveform(i, f);		
	}
	
	delete[] buffer;
	
	finishUndo();	
	
	postFilter();
}

void SampleEditor::tool_triangularSmoothSample(const FilterParameters* par)
{
	if (isEmptySample())
		return;
		
	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_triangularSmoothSample, par);
	
	mp_sint32 sLen = sEnd - sStart;
	
	mp_ubyte* buffer = new mp_ubyte[(sample->type & 16) ? sLen*2 : sLen];
	if (!buffer)
		return;

	memcpy(buffer, sample->sample + sStart, (sample->type & 16) ? sLen*2 : sLen);

	prepareUndo();	
	
	pp_int32 i;

	for (i = sStart; i < sEnd; i++)
	{
		float f = (getFloatSampleFromWaveform(i - sStart - 2, buffer, sLen) +
				  getFloatSampleFromWaveform(i - sStart - 1, buffer, sLen)*2.0f +
				  getFloatSampleFromWaveform(i - sStart, buffer, sLen)*3.0f +
				  getFloatSampleFromWaveform(i - sStart + 1, buffer, sLen)*2.0f +
				  getFloatSampleFromWaveform(i - sStart + 2, buffer, sLen)) * (1.0f/9.0f);
				  
		setFloatSampleInWaveform(i, f);		
	}
	
	delete[] buffer;
	
	finishUndo();	

	postFilter();
}

void SampleEditor::tool_eqSample(const FilterParameters* par)
{
	if (isEmptySample())
		return;
		
	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_eqSample, par);
	
	prepareUndo();	
	
	float c4spd = getc4spd(sample->relnote, sample->finetune);
	float scale = c4spd / 44100.0f;
	
	Equalizer** eqs = new Equalizer*[par->getNumParameters()];
	
	// three band EQ
	if (par->getNumParameters() == 3)
	{
		for (pp_int32 i = 0; i < par->getNumParameters(); i++)
		{
			eqs[i] = new Equalizer();
			eqs[i]->CalcCoeffs(EQConstants::EQ3bands[i]*scale, EQConstants::EQ3bandwidths[i]*scale, c4spd, Equalizer::CalcGain(par->getParameter(i)));
		}
	}
	// ten band EQ
	else if (par->getNumParameters() == 10)
	{
		for (pp_int32 i = 0; i < par->getNumParameters(); i++)
		{
			eqs[i] = new Equalizer();
			eqs[i]->CalcCoeffs(EQConstants::EQ10bands[i]*scale, EQConstants::EQ10bandwidths[i]*scale, c4spd, Equalizer::CalcGain(par->getParameter(i)));
		}
	}
	else
	{
		delete[] eqs;
		finishUndo();
		return;
	}
	
	// apply EQ here
	pp_int32 i;

	for (i = sStart; i < sEnd; i++)
	{
		// Fetch a stereo signal
		double xL = getFloatSampleFromWaveform(i);
		double xR = xL;
			
		for (pp_int32 j = 0; j < par->getNumParameters(); j++)
		{
			double yL, yR;
			// Pass the stereo input
			eqs[j]->Filter(xL, xR, yL, yR);
			
			xL = yL;
			xR = yR;
		}
		
		setFloatSampleInWaveform(i, (float)xL);
	}
	
	for (i = 0; i < par->getNumParameters(); i++)
		delete eqs[i];
	
	delete[] eqs;
	finishUndo();	

	postFilter();
}

void SampleEditor::tool_generateSilence(const FilterParameters* par)
{
	if (isEmptySample())
		return;
		
	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if ((sStart == sEnd) || hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = 0;
	}
	
	preFilter(&SampleEditor::tool_generateSilence, par);
	
	mp_sint32 sLen = sEnd - sStart;
	
	prepareUndo();	
	
	pp_int32 i, j;

	pp_int32 size = (pp_int32)par->getParameter(0);

	pp_int32 newSampleSize = (sample->samplen - sLen) + size;
	
	if (sample->type & 16)
	{		
		mp_sword* dst = new mp_sword[newSampleSize];

		j = 0;
		for (i = 0; i < sStart; i++, j++)
			dst[j] = sample->getSampleValue(i);

		for (i = 0; i < size; i++, j++)
			dst[j] = 0;

		for (i = sEnd; i < (signed)sample->samplen; i++, j++)
			dst[j] = sample->getSampleValue(i);

		module->freeSampleMem((mp_ubyte*)sample->sample);
		sample->sample = (mp_sbyte*)module->allocSampleMem(newSampleSize*2);
		memcpy(sample->sample, dst, newSampleSize*2);

		sample->samplen = newSampleSize;

		delete[] dst;
	}
	else
	{
		mp_sbyte* dst = new mp_sbyte[newSampleSize];

		j = 0;
		for (i = 0; i < sStart; i++, j++)
			dst[j] = sample->getSampleValue(i);

		for (i = 0; i < size; i++, j++)
			dst[j] = 0;

		for (i = sEnd; i < (signed)sample->samplen; i++, j++)
			dst[j] = sample->getSampleValue(i);

		module->freeSampleMem((mp_ubyte*)sample->sample);
		sample->sample = (mp_sbyte*)module->allocSampleMem(newSampleSize);
		memcpy(sample->sample, dst, newSampleSize);

		sample->samplen = newSampleSize;

		delete[] dst;
	}

	// show everything
	lastOperation = OperationCut;
	finishUndo();	

	postFilter();
}

void SampleEditor::tool_generateNoise(const FilterParameters* par)
{
	if (isEmptySample())
		return;
		
	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_generateNoise, par);
	
	prepareUndo();	
	
	pp_int32 i;

	pp_int32 type = (pp_int32)par->getParameter(0);

	VRand rand;
	rand.seed();

	switch (type)
	{
		case 0:
			for (i = sStart; i < sEnd; i++)
				setFloatSampleInWaveform(i, rand.white()*2.0f);		
			break;
		case 1:
			for (i = sStart; i < sEnd; i++)
				setFloatSampleInWaveform(i, rand.pink()*2.0f);		
			break;
		case 2:
			for (i = sStart; i < sEnd; i++)
				setFloatSampleInWaveform(i, rand.brown()*2.0f);		
			break;
	}
	
	finishUndo();	

	postFilter();
}

void SampleEditor::tool_generateSine(const FilterParameters* par)
{
	if (isEmptySample())
		return;
		
	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_generateSine, par);
	
	mp_sint32 sLen = sEnd - sStart;
	
	prepareUndo();	
	
	pp_int32 i;

	const float numPeriods = (float)(6.283185307179586476925286766559 * par->getParameter(1));
	const float amplify = par->getParameter(0);

	// generate sine wave here
	for (i = sStart; i < sEnd; i++)
	{
		float per = (i-sStart)/(float)sLen * numPeriods;
		setFloatSampleInWaveform(i, (float)sin(per)*amplify);	
	}

	finishUndo();	

	postFilter();
}

void SampleEditor::tool_generateSquare(const FilterParameters* par)
{
	if (isEmptySample())
		return;
		
	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_generateSquare, par);
	
	mp_sint32 sLen = sEnd - sStart;
	
	prepareUndo();	
	
	pp_int32 i;

	const float numPeriods = par->getParameter(1);
	const float amplify = par->getParameter(0);

	// generate square wave here
	for (i = sStart; i < sEnd; i++)
	{
		float per = (i-sStart)/(float)sLen * numPeriods;
		float frac = per-(float)floor(per);
		setFloatSampleInWaveform(i, frac < 0.5f ? amplify : -amplify);	
	}

	finishUndo();	

	postFilter();
}

void SampleEditor::tool_generateTriangle(const FilterParameters* par)
{
	if (isEmptySample())
		return;
		
	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_generateTriangle, par);
	
	mp_sint32 sLen = sEnd - sStart;
	
	prepareUndo();	
	
	pp_int32 i;

	const float numPeriods = par->getParameter(1);
	const float amplify = par->getParameter(0);

	// generate triangle wave here
	for (i = sStart; i < sEnd; i++)
	{
		float per = (i-sStart)/(float)sLen * numPeriods;
		float frac = per-(float)floor(per);
		if (frac < 0.25f)
			setFloatSampleInWaveform(i, (frac*4.0f)*amplify);	
		else if (frac < 0.75f)
			setFloatSampleInWaveform(i, (1.0f-(frac-0.25f)*4.0f)*amplify);	
		else	
			setFloatSampleInWaveform(i, (-1.0f+(frac-0.75f)*4.0f)*amplify);	
	}

	finishUndo();	

	postFilter();
}

void SampleEditor::tool_generateSawtooth(const FilterParameters* par)
{
	if (isEmptySample())
		return;
		
	pp_int32 sStart = selectionStart;
	pp_int32 sEnd = selectionEnd;
	
	if (hasValidSelection())
	{
		if (sStart >= 0 && sEnd >= 0)
		{		
			if (sEnd < sStart)
			{
				pp_int32 s = sEnd; sEnd = sStart; sStart = s;
			}
		}
	}
	else
	{
		sStart = 0;
		sEnd = sample->samplen;
	}
	
	preFilter(&SampleEditor::tool_generateSawtooth, par);
	
	mp_sint32 sLen = sEnd - sStart;
	
	prepareUndo();	
	
	pp_int32 i;

	const float numPeriods = par->getParameter(1);
	const float amplify = par->getParameter(0);

	// generate saw-tooth wave here
	for (i = sStart; i < sEnd; i++)
	{
		float per = (i-sStart)/(float)sLen * numPeriods;
		float frac = per-(float)floor(per);
		setFloatSampleInWaveform(i, frac < 0.5f ? (frac*2.0f)*amplify : (-1.0f+((frac-0.5f)*2.0f))*amplify);	
	}

	finishUndo();	

	postFilter();
}

bool SampleEditor::tool_canApplyLastFilter() const
{
	return lastFilterFunc != NULL && isValidSample(); 
}

void SampleEditor::tool_applyLastFilter()
{
	if (lastFilterFunc)
	{
		if (lastParameters)
		{
			FilterParameters newPar(*lastParameters);
			(this->*lastFilterFunc)(&newPar);
		}
		else
		{
			(this->*lastFilterFunc)(NULL);
		}
	}
}

pp_uint32 SampleEditor::convertSmpPosToMillis(pp_uint32 pos, pp_int32 relativeNote/* = 0*/)
{
	if (!isValidSample())
		return 0;
			
	relativeNote+=sample->relnote;
	
	double c4spd = XModule::getc4spd(relativeNote, sample->finetune);
	
	return (pp_uint32)(((double)pos / c4spd) * 1000.0);
}

