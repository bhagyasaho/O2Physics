// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//

#include "Zorro.h"

#include <algorithm>
#include <map>

#include "CCDB/BasicCCDBManager.h"
#include "CommonDataFormat/InteractionRecord.h"

using o2::InteractionRecord;

namespace
{
int findBin(TH1* hist, const std::string& label)
{ // Find bin by label, avoiding the axis extention from the native ROOT implementation
  for (int iBin{1}; iBin <= hist->GetNbinsX(); ++iBin) {
    if (label == hist->GetXaxis()->GetBinLabel(iBin)) {
      return iBin;
    }
  }
  return -1;
}
} // namespace

void Zorro::populateHistRegistry(o2::framework::HistogramRegistry& histRegistry, int runNumber, std::string prefix)
{
  if (mRunNumberHistos == runNumber) {
    return;
  }
  mRunNumberHistos = runNumber;
  if (mSelections) {
    mAnalysedTriggers = histRegistry.add<TH1>((std::to_string(mRunNumberHistos) + "/" + prefix + "AnalysedTriggers").data(), "", o2::framework::HistType::kTH1D, {{mSelections->GetNbinsX() - 2, -0.5, mSelections->GetNbinsX() - 2.5}});
    for (int iBin{2}; iBin < mSelections->GetNbinsX(); ++iBin) { // Exclude first and last bins as they are total number of analysed and selected events, respectively
      mAnalysedTriggers->GetXaxis()->SetBinLabel(iBin - 1, mSelections->GetXaxis()->GetBinLabel(iBin));
    }
    std::shared_ptr<TH1> selections = histRegistry.add<TH1>((std::to_string(mRunNumberHistos) + "/" + prefix + "Selections").data(), "", o2::framework::HistType::kTH1D, {{mSelections->GetNbinsX(), -0.5, static_cast<double>(mSelections->GetNbinsX() - 0.5)}});
    for (int iBin{1}; iBin <= mSelections->GetNbinsX(); ++iBin) {
      selections->GetXaxis()->SetBinLabel(iBin, mSelections->GetXaxis()->GetBinLabel(iBin));
      selections->SetBinContent(iBin, mSelections->GetBinContent(iBin));
      selections->SetBinError(iBin, mSelections->GetBinError(iBin));
    }
  }
  if (mScalers) {
    std::shared_ptr<TH1> scalers = histRegistry.add<TH1>((std::to_string(mRunNumberHistos) + "/" + prefix + "Scalers").data(), "", o2::framework::HistType::kTH1D, {{mScalers->GetNbinsX(), -0.5, static_cast<double>(mScalers->GetNbinsX() - 0.5)}});
    for (int iBin{1}; iBin <= mScalers->GetNbinsX(); ++iBin) {
      scalers->GetXaxis()->SetBinLabel(iBin, mScalers->GetXaxis()->GetBinLabel(iBin));
      scalers->SetBinContent(iBin, mScalers->GetBinContent(iBin));
      scalers->SetBinError(iBin, mScalers->GetBinError(iBin));
    }
  }
  if (mInspectedTVX) {
    std::shared_ptr<TH1> inspectedTVX = histRegistry.add<TH1>((std::to_string(mRunNumberHistos) + "/" + prefix + "InspectedTVX").data(), "", o2::framework::HistType::kTH1D, {{mInspectedTVX->GetNbinsX(), -0.5, static_cast<double>(mInspectedTVX->GetNbinsX() - 0.5)}});
    for (int iBin{1}; iBin <= mInspectedTVX->GetNbinsX(); ++iBin) {
      inspectedTVX->GetXaxis()->SetBinLabel(iBin, mInspectedTVX->GetXaxis()->GetBinLabel(iBin));
      inspectedTVX->SetBinContent(iBin, mInspectedTVX->GetBinContent(iBin));
      inspectedTVX->SetBinError(iBin, mInspectedTVX->GetBinError(iBin));
    }
  }
  if (mTOIs.size()) {
    mAnalysedTriggersOfInterest = histRegistry.add<TH1>((std::to_string(mRunNumberHistos) + "/" + prefix + "AnalysedTriggersOfInterest").data(), "", o2::framework::HistType::kTH1D, {{static_cast<int>(mTOIs.size()), -0.5, static_cast<double>(mTOIs.size() - 0.5)}});
    for (size_t i{0}; i < mTOIs.size(); ++i) {
      mAnalysedTriggersOfInterest->GetXaxis()->SetBinLabel(i + 1, mTOIs[i].data());
    }
  }
}

std::vector<int> Zorro::initCCDB(o2::ccdb::BasicCCDBManager* ccdb, int runNumber, uint64_t timestamp, std::string tois, int bcRange)
{
  if (mRunNumber == runNumber) {
    return mTOIidx;
  }
  mCCDB = ccdb;
  mRunNumber = runNumber;
  mBCtolerance = bcRange;
  std::map<std::string, std::string> metadata;
  metadata["runNumber"] = std::to_string(runNumber);
  mScalers = mCCDB->getSpecific<TH1D>(mBaseCCDBPath + "FilterCounters", timestamp, metadata);
  mSelections = mCCDB->getSpecific<TH1D>(mBaseCCDBPath + "SelectionCounters", timestamp, metadata);
  mInspectedTVX = mCCDB->getSpecific<TH1D>(mBaseCCDBPath + "InspectedTVX", timestamp, metadata);
  mZorroHelpers = mCCDB->getSpecific<std::vector<ZorroHelper>>(mBaseCCDBPath + "ZorroHelpers", timestamp, metadata);
  std::sort(mZorroHelpers->begin(), mZorroHelpers->end(), [](const auto& a, const auto& b) { return std::min(a.bcAOD, a.bcEvSel) < std::min(b.bcAOD, b.bcEvSel); });
  mBCranges.clear();
  for (auto helper : *mZorroHelpers) {
    mBCranges.emplace_back(InteractionRecord::long2IR(std::min(helper.bcAOD, helper.bcEvSel)), InteractionRecord::long2IR(std::max(helper.bcAOD, helper.bcEvSel)));
  }

  mLastBCglobalId = 0;
  mLastSelectedIdx = 0;
  mTOIs.clear();
  mTOIidx.clear();
  while (!tois.empty()) {
    size_t pos = tois.find(",");
    pos = (pos == std::string::npos) ? tois.size() : pos;
    std::string token = tois.substr(0, pos);
    // Trim leading and trailing whitespaces from the token
    token.erase(0, token.find_first_not_of(" "));
    token.erase(token.find_last_not_of(" ") + 1);
    int bin = findBin(mSelections, token) - 2;
    mTOIs.push_back(token);
    mTOIidx.push_back(bin);
    tois = tois.erase(0, pos + 1);
  }
  mTOIcounts.resize(mTOIs.size(), 0);
  LOGF(info, "Zorro initialized for run %d, triggers of interest:", runNumber);
  for (size_t i{0}; i < mTOIs.size(); ++i) {
    LOGF(info, ">>> %s : %i", mTOIs[i].data(), mTOIidx[i]);
  }
  return mTOIidx;
}

std::bitset<128> Zorro::fetch(uint64_t bcGlobalId, uint64_t tolerance)
{
  uint64_t lastSelectedIdx = mLastSelectedIdx;
  mLastResult.reset();
  o2::dataformats::IRFrame bcFrame{InteractionRecord::long2IR(bcGlobalId) - tolerance, InteractionRecord::long2IR(bcGlobalId) + tolerance};
  if (bcGlobalId < mLastBCglobalId) {
    mLastSelectedIdx = 0;
  }
  mLastBCglobalId = bcGlobalId;
  for (size_t i = mLastSelectedIdx; i < mBCranges.size(); i++) {
    if (!mBCranges[i].isOutside(bcFrame)) {
      for (int iMask{0}; iMask < 2; ++iMask) {
        for (int iTOI{0}; iTOI < 64; ++iTOI) {
          if (mZorroHelpers->at(i).selMask[iMask] & (1ull << iTOI)) {
            mLastResult.set(iMask * 64 + iTOI, 1);
            if (mAnalysedTriggers && i != lastSelectedIdx) {
              mAnalysedTriggers->Fill(iMask * 64 + iTOI);
            }
          }
        }
      }
      mLastSelectedIdx = i;
      return mLastResult;
    } else if (mBCranges[i].getMax() < bcFrame.getMin()) {
      mLastSelectedIdx = i;
    } else if (mBCranges[i].getMin() > bcFrame.getMax()) {
      break;
    }
  }
  return mLastResult;
}

bool Zorro::isSelected(uint64_t bcGlobalId, uint64_t tolerance)
{
  uint64_t lastSelectedIdx = mLastSelectedIdx;
  fetch(bcGlobalId, tolerance);
  for (size_t i{0}; i < mTOIidx.size(); ++i) {
    if (mTOIidx[i] < 0) {
      continue;
    } else if (mLastResult.test(mTOIidx[i])) {
      mTOIcounts[i] += (lastSelectedIdx != mLastSelectedIdx); /// Avoid double counting
      if (mAnalysedTriggersOfInterest && lastSelectedIdx != mLastSelectedIdx) {
        mAnalysedTriggersOfInterest->Fill(i);
      }
      return true;
    }
  }
  return false;
}
