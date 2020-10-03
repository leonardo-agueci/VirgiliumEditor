//
// Created by alex on 07/12/19.
//

#include "virgilium_client.h"
#include "../common/CRDT/Symbol.h"
#include <algorithm>
#include <iostream>
#include <utility>
#include <common/messages/StorageMessage.h>

virgilium_client::virgilium_client(QWidget *parent, ClientSocket *receivedSocket) : QDialog(parent) {
    this->socket = receivedSocket;
    this->_counter = 0;

    connect(this->socket, &ClientSocket::storageMessageReceivedLoad, this, &virgilium_client::loadResponse);
    //connect(this->socket, &ClientSocket::basicMessageReceived, this,&virgilium_client::site_id_assignment);
    //connect(this,&virgilium_client::site_id_assignment,this->server,&server_virgilium::site_id_assignment);
    this->_siteId=this->socket->getClientID();
    qDebug()<<"Il mio siteID è: " << this->_siteId;
}

virgilium_client::~virgilium_client() {
    //delete(this->server);
}

_int virgilium_client::get_id() const {
    return this->_siteId;
}

QString virgilium_client::to_string() {
    QString buffer;
    std::for_each(this->_symbols.begin(), this->_symbols.end(), [&buffer](Symbol s) {
        buffer.push_back(s.getLetter());
    });
    return buffer;
}

//TODO
void virgilium_client::process(const CrdtMessage &m) {

    // "ERASE" "INSERT"
    auto s = m.getSymbol();
    _int i, index;
    bool flagUguale = false;
    if (m.getAction() == "INSERT") {

        auto nuovaPos = m.getSymbol().getPosition();
        /*for (_int i = 0; i < this->_symbols.size(); i++) {
            if (this->_symbols[i].getPosition() == nuovaPos)
                //throw wrongpositionException();
                ;//throw wrongpositionException();
        }*/

        for (i = 0; i < this->_symbols.size(); i++)
            if (s <= this->_symbols[i]) break; // ho trovato il mio i;
        auto it = this->_symbols.begin() + i;
        this->_symbols.insert(it, s);

        emit insert_into_window(i, s.getLetter(), s.getFont());
    } else {
        for (i = 0; i < this->_symbols.size(); i++) //scorro ogni simbolo all'interno del documento
            if (s == this->_symbols[i]) break;
        auto it = this->_symbols.begin() + i;
        this->_symbols.erase(it);
        /*this->_symbols.erase(std::remove_if(this->_symbols.begin(),this->_symbols.end(), [&s](symbol s1){ return s==s1; }));*/
        emit remove_into_window(i);
    }

    return;
}

/* Aggiunto per cursore */
void virgilium_client::changeCursorPosition(const CrdtMessage &m) {
    emit change_cursor_position(m.getSymbol().getPosition().at(1), m.getFrom());
}

//gli interi prev e next sono quelli che andranno a contenere quelli che sono gli elementi passati alla funzione
//i due vettori vengono controllati parallelemante
//_max gli si assegna il la dimensione maggiore tra i due vettori
QVector<_int> virgilium_client::getPosition(QVector<_int> prec, QVector<_int> succ) {
    QVector<_int> nuovaPos;
    _int i, prev, next, _max = (prec.size() > succ.size()) ? prec.size() : succ.size();
    for (i = 0; i < _max; i++) {
        //size Ã¨ finito? se Ã¨ finito non ci voglio accedere (potrei avere un errore)
        //se un vettore non Ã¨ ancora arrivato alla fine, mi prendo il suo valore
        //se un vettore Ã¨ finito gli assegno zero
        prev = (i >= prec.size()) ? 0 : prec[i];
        next = (i >= succ.size()) ? 9 : succ[i];

        if (prev == next) {
            nuovaPos.push_back(prev);
        } else {
            if (next - prev >= 2) {
                nuovaPos.push_back(prev + 1);
                break;
            } else {
                // aggiungiamo il caso in cui prev > next
                if (prev > next) {

                    // se prev != 9 allora possiamo incrementarlo
                    if (prev != 9) {
                        nuovaPos.push_back(prev + 1);
                        break;
                    } else if (next != 0) { // se prev = 9 allora proviamo a decrementare next
                        nuovaPos.push_back(next - 1);
                        break;
                    }
                }
                nuovaPos.push_back(prev);

            }

        }
    }
    if (i == _max) {
        nuovaPos.push_back(1);
    }
    return nuovaPos;
}

/* This method is used to send to other clients the new position of my cursor. */
void virgilium_client::changeCursor(_int position) {
    QVector <_int> pos = {position - 1, position};
    Symbol::CharFormat font = Symbol::CharFormat();
    Symbol s("", this->_siteId, this->_counter, pos, font);
    CrdtMessage m(0, s, this->_siteId, "CURSOR_CHANGED");
    //this->socket->sendCrdt(CURSOR_CHANGED, m);
}

/* This method is used to say to other clients that a char is deleted. */
void virgilium_client::localErase(_int index) {
    auto s = this->_symbols[index];
    auto it = this->_symbols.begin() + index;
    this->_symbols.erase(it);
    CrdtMessage m(0, s, this->_siteId, "ERASE");
    this->socket->send(SYMBOL_INSERT_OR_ERASE,m);
}

/* This method is used to say to other clients that a char is inserted. */
void virgilium_client::localInsert(_int index, QString value, Symbol::CharFormat font) {
    QVector <_int> prec; //= this->_symbols[index-1];
    QVector <_int> nuovaPos;
    QVector <_int> succ; //auto succ //= this-> _symbols[index];

    if (this->_symbols.empty() && index == 0) {
        //primo elemento inserito
        prec.push_back(0);
        succ.push_back(2);
        // nuovaPos.push_back(1);// lo zero non va mai messo come posizione
    } else {
        if (index == 0) {
            //inserisco in testa
            //std::vector<int> zeroes;
            prec.push_back(0);
            succ = this->_symbols[index].getPosition();
        }
        if (index == this->_symbols.size()) {
            //ultimo a destra
            prec = this->_symbols[index - 1].getPosition();
            succ.push_back(prec[0] + 2);
            // faccio prec[0]+2 così la mia funzione getPosition dovrebbe ficcare
            // il newSymbol symbol tra i due senza creare un vettorone ma creando un
            // vettore di un elemento di valore prec[0]+1 (spero)
        }
        if (index != 0 && index != this->_symbols.size()) {
            //caso medio
            //    std::cout<<"sto inserendo " << value<<std::endl;
            prec = this->_symbols[index - 1].getPosition();
            succ = this->_symbols[index].getPosition();
        }
    }

    nuovaPos = this->getPosition(prec, succ);

    Symbol newSymbol(std::move(value), this->_siteId, this->_counter++, nuovaPos, std::move(font));
    auto it = this->_symbols.begin() + index;
    this->_symbols.insert(it, newSymbol);
    CrdtMessage m(0, newSymbol, this->_siteId, "INSERT");
    //m.printMessage();
    this->socket->send(SYMBOL_INSERT_OR_ERASE,m); //TODO da cambiare per bene
}


void virgilium_client::set_site_id(qint64 siteId) {
    this->_siteId = siteId;
    qDebug() << "Il mio site ID e' : " << this->_siteId << "\n";
}

void virgilium_client::loadRequest(const QString &fileName, User user) {
    user.setSiteId(this->_siteId);
    user.setLastCursorPos(0);

    QList<User> users = {user};
    QVector<Symbol> symbols;
    StorageMessage storageMessage(this->_siteId, symbols, fileName, users);
    this->socket->sendStorage(LOAD_REQUEST, storageMessage);
}

void virgilium_client::loadResponse(StorageMessage storageMessage) {
    for (const Symbol &symbol : storageMessage.getSymbols())
        this->_symbols.push_back(symbol);

    QList<User> users;
    for (User u : storageMessage.getActiveUsers()) {
        u.setAssignedColor(QColor(QRandomGenerator::global()->bounded(64, 192),
                                  QRandomGenerator::global()->bounded(64, 192),
                                  QRandomGenerator::global()->bounded(64, 192)));
        users.push_back(u);
    }

    emit load_response(storageMessage.getSymbols(), users);
}

void virgilium_client::save(QString fileName) {
    QVector<Symbol> symbols;
    for (const auto &_symbol : this->_symbols) {
        symbols.push_back(_symbol);
    }
    QList<User> users;
    StorageMessage storageMessage(this->_siteId, symbols, std::move(fileName), users);
    this->socket->sendStorage(SAVE, storageMessage);
}

void virgilium_client::deleteFromActive(const User &user, const QString &fileName) {
    UserMessage userMessage(this->_siteId, user, fileName);
    this->socket->send(DELETE_ACTIVE, userMessage);
}

