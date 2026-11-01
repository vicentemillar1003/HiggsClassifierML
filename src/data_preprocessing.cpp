/*
Extrae las variables de interés del archivo ROOT producido por MadGraph y las escribe en un CSV
para su posterior uso.

Compilación: g++ -O2 -std=c++17 extract_diphoton.cpp -o extract_diphoton $(root-config --cflags --libs)
Uso: ./extract_diphoton archivo.root [salida.csv] [nombre_arbol]

Si en un evento hay < 2 fotones, las variables que dependen de
ambos fotones como photon2_*, m_gg, diphoton_*, delta_r_gg toman el
valor -999. y si photon1_* se escribe como -999 si no hay ningún fotón.
*/

#include <TFile.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>
#include <TTreeReaderArray.h>
#include <TLorentzVector.h>
#include <TVector2.h>
#include <TError.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

static const double FILL = -999.0;

int main(int argc, char** argv)
{
    gErrorIgnoreLevel = kError;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " file.root [output.csv]\n";
        return 1;
    }

    const std::string inName   = argv[1];
    const std::string outName  = (argc > 2) ? argv[2] : "processed_unknown_data.csv";
    const std::string treeName = "Delphes";

    // Abrir archivo y tree
    std::unique_ptr<TFile> file(TFile::Open(inName.c_str(), "READ"));
    if (!file || file->IsZombie()) {
        std::cerr << "Error: could not open '" << inName << "'\n";
        return 1;
    }

    TTree* tree = dynamic_cast<TTree*>(file->Get(treeName.c_str()));
    if (!tree) {
        std::cerr << "Error: tree '" << treeName << "' not found in the file.\n"
                  << "Available trees:\n";
        file->ls();
        return 1;
    }

    // Comprobar si existen las branches necesarias
    for (const char* b : {"Photon.PT", "Photon.Eta", "Photon.Phi", "Photon_size",
                          "Jet_size", "MissingET.MET"}) {
        if (!tree->GetBranch(b)) {
            std::cerr << "Error: missing branch '" << b
                      << "'. Is this a Delphes file??\n";
            return 1;
        }
    }
    const bool hasWeight = (tree->GetBranch("Event.Weight") != nullptr);
    if (!hasWeight)
        std::cerr << "Warning: 'Event.Weight' does not exist; using weight = 1.\n";

    // Readers
    TTreeReader reader(tree);
    TTreeReaderArray<Float_t> phPT (reader, "Photon.PT");
    TTreeReaderArray<Float_t> phEta(reader, "Photon.Eta");
    TTreeReaderArray<Float_t> phPhi(reader, "Photon.Phi");
    TTreeReaderValue<Int_t>   nPh  (reader, "Photon_size");
    TTreeReaderValue<Int_t>   nJet (reader, "Jet_size");
    TTreeReaderArray<Float_t> met  (reader, "MissingET.MET");

    std::unique_ptr<TTreeReaderArray<Float_t>> weight;
    if (hasWeight)
        weight = std::make_unique<TTreeReaderArray<Float_t>>(reader, "Event.Weight");

    // csv
    std::ofstream out(outName);
    if (!out) {
        std::cerr << "Error: could not create '" << outName << "'\n";
        return 1;
    }
    out << std::setprecision(8);
    out << "photon1_pt,photon1_eta,photon1_phi,"
           "photon2_pt,photon2_eta,photon2_phi,"
           "m_gg,diphoton_pt,diphoton_eta,delta_r_gg,"
           "n_photons,n_jets,missing_et,event_weight\n";

    // Iteración en los eventos
    Long64_t nEvents = 0, nWith2 = 0;

    while (reader.Next()) {
        ++nEvents;

        const int n = *nPh;

        // Indices de los dos fotones de mayor pT
        int i1 = -1, i2 = -1;
        for (int i = 0; i < n; ++i) {
            if (i1 < 0 || phPT[i] > phPT[i1]) {
                i2 = i1;
                i1 = i;
            } else if (i2 < 0 || phPT[i] > phPT[i2]) {
                i2 = i;
            }
        }

        double pt1 = FILL, eta1 = FILL, phi1 = FILL;
        double pt2 = FILL, eta2 = FILL, phi2 = FILL;
        double mgg = FILL, dpt = FILL, deta = FILL, dR = FILL;

        if (i1 >= 0) {
            pt1 = phPT[i1]; eta1 = phEta[i1]; phi1 = phPhi[i1];
        }
        if (i1 >= 0 && i2 >= 0) {
            pt2 = phPT[i2]; eta2 = phEta[i2]; phi2 = phPhi[i2];

            TLorentzVector g1, g2;
            g1.SetPtEtaPhiM(pt1, eta1, phi1, 0.0); // fotones sin masa :P
            g2.SetPtEtaPhiM(pt2, eta2, phi2, 0.0);
            const TLorentzVector sys = g1 + g2;

            mgg  = sys.M();
            dpt  = sys.Pt();
            deta = sys.Eta();

            const double dEta = eta1 - eta2;
            const double dPhi = TVector2::Phi_mpi_pi(phi1 - phi2);
            dR = std::sqrt(dEta * dEta + dPhi * dPhi);

            ++nWith2;
        }

        const double missingET = (met.GetSize() > 0) ? met[0] : FILL;
        const double w = (weight && weight->GetSize() > 0) ? (*weight)[0] : 1.0;

        out << pt1 << ',' << eta1 << ',' << phi1 << ','
            << pt2 << ',' << eta2 << ',' << phi2 << ','
            << mgg << ',' << dpt << ',' << deta << ',' << dR << ','
            << n << ',' << *nJet << ',' << missingET << ',' << w << '\n';
    }

    out.close();

    std::cout << "Events processed               : " << nEvents << '\n'
              << "Events with more than 1 photon : " << nWith2  << '\n'
              << "CSV written to                 : " << outName << '\n';
    return 0;
}