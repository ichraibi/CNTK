//
// <copyright file="SequenceReader.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//
// SequenceReader.cpp : Defines the exported functions for the DLL application.
//


#include "stdafx.h"
#define DATAREADER_EXPORTS  // creating the exports here
#include "HTXSequenceReader.h"
#ifdef LEAKDETECT
#include <vld.h> // leak detection
#endif
#include <iostream>
#include <vector>
#include <string>

namespace Microsoft { namespace MSR { namespace CNTK {

template<class ElemType>
void BatchSequenceReader<ElemType>::ReadClassInfo(const wstring & vocfile, int& class_size,
    map<string, int>& word4idx,
    map<int, string>& idx4word,
    map<int, int>& idx4class,
    map<int, size_t> & idx4cnt,
    int nwords,
    string mUnk,
    bool /*flatten*/)
{
    string tmp_vocfile(vocfile.begin(), vocfile.end()); // convert from wstring to string
    string strtmp;
    size_t cnt;
    int clsidx, b;
    class_size = 0;

    string line;
    vector<string> tokens;
    ifstream fin;
    fin.open(tmp_vocfile.c_str());
    if (!fin)
    {
        RuntimeError("cannot open word class file");
    }

    while (getline(fin, line))
    {
        line = trim(line);
        tokens = msra::strfun::split(line, "\t ");
        assert(tokens.size() == 4);

        b = stoi(tokens[0]);
        cnt = (size_t)stof(tokens[1]);
        strtmp = tokens[2];
        clsidx = stoi(tokens[3]);

        idx4cnt[b] = cnt;
        word4idx[strtmp] = b;
        idx4word[b] = strtmp;

        idx4class[b] = clsidx;
        class_size = max(class_size, clsidx);
    }
    fin.close();
    class_size++;

    if (idx4class.size() < nwords)
    {
        LogicError("SequenceReader::ReadClassInfo the actual number of words %d is smaller than the specified vocabulary size %d. Check if labelDim is too large. ", idx4class.size(), nwords);
    }
    std::vector<double> counts(idx4cnt.size());
    for (auto p : idx4cnt)
        counts[p.first] = (double)p.second;

    /// check if unk is the same used in vocabulary file
    if (word4idx.find(mUnk.c_str()) == word4idx.end())
    {
        LogicError("SequenceReader::ReadClassInfo unk symbol %s is not in vocabulary file", mUnk.c_str());
    }
}

template<class ElemType>
bool BatchSequenceReader<ElemType>::refreshCacheSeq(int seq_id)
{
    if (sequence_cache[seq_id]->size() > 0)
        return true;
    bool res = false;
    if (!fin.is_open())
        return false;
    string word;
    for (;;) {
        if (!(fin >> word)) {
            fin.close();
            return false;
        }
        int word_id = word4idx[word];
        sequence_cache[seq_id]->push_back(word_id);
        res = true;
        if (word_id == sentenceEndId && sequence_cache[seq_id]->size() > 1) { //Meet a sentence End
            break;
        }
    }
    return res;
}

template<class ElemType>
void BatchSequenceReader<ElemType>::Init(const ConfigParameters& readerConfig)
{
    fprintf(stderr, "debughtx ---LMHTXSequenceReader Init---\n");
    system("sleep 0.1");

    mBlgSize = readerConfig("nbruttsineachrecurrentiter", "1");
    fprintf(stderr, "debughtx sequence number is %d\n", mBlgSize);
    nwords = readerConfig("vocabsize", "0");
    if (nwords == 0) {
        RuntimeError("[LMHTXSequenceReader] vocabsize option not set.");
    }
    
    mUnk = readerConfig("unk", "<unk>");
    std::wstring wClassFile = readerConfig("wordclass", "");
    if (wClassFile.compare(L"") == 0) {
        RuntimeError("[LMHTXSequenceReader] wordclass option not set.");
    }
    std::wstring temp_s = readerConfig("file");
    fileName = std::string(temp_s.begin(), temp_s.end());

    debughtx = readerConfig("debughtx", "1");
    if (debughtx == 1)
        fprintf(stderr, "debughtx set to one, will give a lot of debug output....\n");

    ReadClassInfo(wClassFile, class_size,
        word4idx,
        idx4word,
        idx4class,
        idx4cnt,
        nwords,
        mUnk,
        false);

    sentenceEndId = word4idx["</s>"];
    fprintf(stderr, "debughtx sentenceEndId is %d\n", sentenceEndId);

    fprintf(stderr, "debughtx ---LMHTXSequenceReader end---\n");
}

template<class ElemType>
void BatchSequenceReader<ElemType>::Reset()
{
    DEBUG_HTX fprintf(stderr, "debughtx ---BatchSequenceReader<ElemType>::Reset() called---");
    //TODO clear some memory
    DEBUG_HTX fprintf(stderr, "debughtx ---BatchSequenceReader<ElemType>::Reset() ended---");
}

template<class ElemType>
void BatchSequenceReader<ElemType>::StartMinibatchLoop(size_t mbSize, size_t epoch, size_t requestedEpochSamples)
{
    DEBUG_HTX fprintf(stderr, "debughtx --void HTXBatchSequenceReader<ElemType>::StartMinibatchLoop called---\n");
    DEBUG_HTX fprintf(stderr, "mbSize:%d epoch:%d requestedEpochSamples:%d\n", mbSize, epoch, requestedEpochSamples); //requestedEpochSamples will be -1 when epochSize=0
    m_mbSize = mbSize; //Size of minibatch requested
    fprintf(stderr, "debughtx StartMinibatchLoop MBSize is %d\n", m_mbSize);
    fprintf(stderr, "debughtx StartMinibatchLoop sequenceSize is %d\n", mBlgSize);
    DEBUG_HTX fprintf(stderr, "fileName:%s\n", fileName.c_str());
    fin.open(fileName);

    sequence_cache.clear();
    for (int i = 0; i < mBlgSize; i++)
        sequence_cache.push_back(new list<int>());

    minibatchFlag.TransferFromDeviceToDevice(minibatchFlag.GetDeviceId(), CPUDEVICE, false, true, false); //This matrix lies in CPU, because I want to use SetValue(i,j)

    //EnsureDataAvailable(0); //debughtx, just for debug!

    DEBUG_HTX fprintf(stderr, "debughtx ---void HTXBatchSequenceReader<ElemType>::StartMinibatchLoop ended---\n");
}

template<class ElemType>
size_t BatchSequenceReader<ElemType>::NumberSlicesInEachRecurrentIter()
{
    DEBUG_HTX fprintf(stderr, "debughtx ---BatchSequenceReader<ElemType>::NumberSlicesInEachRecurrentIter called---\n");
    //fprintf(stderr, "debughtx returning %d\n", mBlgSize);
    DEBUG_HTX fprintf(stderr, "debughtx ---BatchSequenceReader<ElemType>::NumberSlicesInEachRecurrentIter ended---\n");
    return mBlgSize;
}

template<class ElemType>
bool BatchSequenceReader<ElemType>::GetMinibatch(std::map<std::wstring, Matrix<ElemType>*>& matrices)
{
    DEBUG_HTX fprintf(stderr, "debughtx ---LMHTXSequenceReader::GetMinibatch called---\n");
    //features idx2cls labels
    Matrix<ElemType>* feature_m = matrices[L"features"];
    Matrix<ElemType>* label_m = matrices[L"labels"];
    label_m_ref = label_m;
    //All zero!
    size_t wordNumber = mBlgSize * m_mbSize;

    DEVICEID_TYPE featureDeviceId = feature_m->GetDeviceId(); //SetValue(i,j,value) need to be called when the matrix is on CPU
    DEVICEID_TYPE labelDeviceId = label_m->GetDeviceId(); //SetValue(i,j,value) need to be called when the matrix is on CPU
    feature_m->TransferFromDeviceToDevice(featureDeviceId, CPUDEVICE, false, true, false); 
    label_m->TransferFromDeviceToDevice(labelDeviceId, CPUDEVICE, false, true, false);

    feature_m->Resize(nwords, wordNumber, wordNumber);
    feature_m->Reset();
    
    /*
    for (int i = 0; i < wordNumber; i++) {
        fprintf(stderr, "1\n");
        feature_m->SetValue(0, i, (ElemType)2); //All word 0! This is for stupid version, should be deleted for simple version
        fprintf(stderr, "2\n");
        feature_m->SetValue(0, i, (ElemType)2); //All word 0! This is for stupid version, should be deleted for simple version
        fprintf(stderr, "3\n");
    }
    */
    label_m->Resize(4, wordNumber);
    //label_m->Reset(); //Can't do this 

    bool res = false; //Got something new?
    minibatchFlag.Resize(mBlgSize, m_mbSize);

    int *temp_feature =  new int[wordNumber];

    for (int i = 0; i < mBlgSize; i++) {
        bool end = false;
        for (int j = 0; j < m_mbSize; j++) {
            minibatchFlag.SetValue(i, j, (ElemType)MinibatchPackingFlag::None);
            int idx = j * (int)mBlgSize + i;
            if (!refreshCacheSeq(i))  //can we have new words in the cache for stream i?
                end = true;
            if (end) {
                temp_feature[idx] = 0; //feature_m->SetValue(0, idx, (ElemType)1); //The rubbish will be filtered out by ComputationNode<ElemType>::MaskToZeroWhenLabelAndFeatureMissing
                label_m->SetValue(0, idx, (ElemType)1); //The rubbish will be filtered out by ComputationNode<ElemType>::MaskToZeroWhenLabelAndFeatureMissing
                minibatchFlag.SetValue(i, j, (ElemType)(MinibatchPackingFlag::NoInput)); //Rubbish here
                continue;
            }
            if (sequence_cache[i]->size() < 2) {
                LogicError("Error in BatchSequenceReader<ElemType>::GetMinibatch, sequence_cache[%d]->size() < 2, it should be always >= 2.", i);
            }
            //fprintf(stderr, "debughtx feature_m->SetValue seq i:%d mbCol j:%d row:%d col:%d matrixrow:%d matrixcol:%d\n", i, j, sequence_cache[i]->front(), idx, feature_m->GetNumRows(), feature_m->GetNumCols()); //added when meet a bug crash
            temp_feature[idx] = sequence_cache[i]->front(); //feature_m->SetValue(sequence_cache[i]->front(), idx, (ElemType)1);
            if (sequence_cache[i]->front() == sentenceEndId) //Beginning of a sentence
                minibatchFlag.SetValue(i, j, (ElemType)MinibatchPackingFlag::SequenceStart);
            res = true; //Got some word new
            sequence_cache[i]->pop_front();
            label_m->SetValue(0, idx, (ElemType)sequence_cache[i]->front());
            if (sequence_cache[i]->front() == sentenceEndId) { //End of a sentence, pop it out for a new one.
                sequence_cache[i]->pop_front();
                minibatchFlag.SetValue(i, j, (ElemType)MinibatchPackingFlag::SequenceEnd);
                end = true;
            }
        }
    }

    for (int i = 0; i < wordNumber; i++)
        feature_m->SetValue(temp_feature[i], i, (ElemType)1); //The assigning to a sparse matrix need to follow a strict order!!!
    delete temp_feature;

    feature_m->TransferFromDeviceToDevice(CPUDEVICE, featureDeviceId, false, false, false); //Done, move it back to GPU if necessary
    label_m->TransferFromDeviceToDevice(CPUDEVICE, labelDeviceId, false, false, false); //Done, move it back to GPU if necessary

    DEBUG_HTX PrintMinibatch(matrices); //Just for debughtx

    DEBUG_HTX fprintf(stderr, "debughtx ---LMHTXSequenceReader::GetMinibatch ended, returning res is %d---\n", res);
    DEBUG_HTX system("sleep 0.5");
    return res;
}

template<class ElemType>
void BatchSequenceReader<ElemType>::PrintMinibatch(std::map<std::wstring, Matrix<ElemType>*>& matrices) {
    fprintf(stderr, "debughtx LMHTXSequenceReader::PrintMinibatch matrix(label) row:%d col:%d\n", matrices.find(L"labels")->second->GetNumRows(), matrices.find(L"labels")->second->GetNumCols());
    int sequence_number = (int)NumberSlicesInEachRecurrentIter();
    for (int i = 0; i < matrices.find(L"labels")->second->GetNumCols(); i++) {
        for (int j = 0; j < 4; j++) {
            fprintf(stderr, " %.2lf", (*(matrices.find(L"labels")->second))(j, i));
            if (j == 0)
                fprintf(stderr, "[%s] ", idx4word[int((*(matrices.find(L"labels")->second))(j, i))].c_str());
            if ((j == 3) && ((i + 1) % sequence_number == 0))
                fprintf(stderr, "\n");
        }
    }
    system("sleep 1");
}

template<class ElemType>
bool BatchSequenceReader<ElemType>::DataEnd(EndDataType endDataType) //This function is obselete
{
    bool ret = false;
    if (endDataType == EndDataType::endDataNull)
        ret = false; //non sense code
    DEBUG_HTX fprintf(stderr, "debughtx ---BatchSequenceReader<ElemType>::DataEnd called, will do nothing---\n");
    DEBUG_HTX fprintf(stderr, "debughtx ---BatchSequenceReader<ElemType>::DataEnd ended---\n");
    return ret;
}

template<class ElemType>
void BatchSequenceReader<ElemType>::SetSentenceSegBatch(Matrix<ElemType>& sentenceBegin, vector<MinibatchPackingFlag>& minibatchPackingFlag)
{
    //static bool first = true; //Stupid version debughtx
    DEBUG_HTX fprintf(stderr, "debughtx ---SetSentenceSegBatch called---\n");
    //For the stupid version, I need to set it to sequenceStart everything, otherwise the first pastActivity for the recurrent node will be wrong in dimension.
    sentenceBegin.Resize(mBlgSize, m_mbSize);
    sentenceBegin.SetValue(0);
    minibatchPackingFlag.resize(m_mbSize);

    minibatchFlag.TransferFromDeviceToDevice(CPUDEVICE, sentenceBegin.GetDeviceId());
    sentenceBegin.SetValue(minibatchFlag);
    minibatchFlag.TransferFromDeviceToDevice(sentenceBegin.GetDeviceId(), CPUDEVICE);
    /* //Stupid version debughtx
    std::fill(minibatchPackingFlag.begin(), minibatchPackingFlag.end(), MinibatchPackingFlag::None);
    if (first) {
        for (int i = 0; i < mBlgSize; i++) {
            sentenceBegin.SetValue(i, 0, (ElemType)1);
        }
        minibatchPackingFlag[0] = MinibatchPackingFlag::SequenceStart;
        first = false;
    }
    */
    
    for (int i = 0; i < m_mbSize; i++) {
        int k = (int)MinibatchPackingFlag::None;
        for (int j = 0; j < mBlgSize; j++)
            k |= (int)minibatchFlag(j, i);
        minibatchPackingFlag[i] = (MinibatchPackingFlag)k;
    }

    DEBUG_HTX {
        //print debug info
        fprintf(stderr, "debughtx matrix sentenceBegin row:%d col:%d\n", sentenceBegin.GetNumRows(), sentenceBegin.GetNumCols());
        for (int i = 0; i < sentenceBegin.GetNumRows(); i++) {
            for (int j = 0; j < sentenceBegin.GetNumCols(); j++)
                fprintf(stderr, "%.2lf ", sentenceBegin(i, j));
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "debughtx vector minibatchPackingFlag size:%d\n", minibatchPackingFlag.size());
        for (int i = 0; i < minibatchPackingFlag.size(); i++)
            fprintf(stderr, "%d ", minibatchPackingFlag.at(i));
        fprintf(stderr, "\n");
    }

    DEBUG_HTX fprintf(stderr, "debughtx ---SetSentenceSegBatch ended---\n");
    DEBUG_HTX system("sleep 0.5");
}

template class BatchSequenceReader<double>; 
template class BatchSequenceReader<float>;
}}}
