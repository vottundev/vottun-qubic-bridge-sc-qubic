﻿#pragma once

#include "qpi.h"

using namespace QPI;


struct ETHBRIDGE : public ContractBase {

public:
    // Bridge Order Structure
    struct BridgeOrder {
        uint64 orderId;                      // Unique ID for the order
        id qubicSender;                      // Sender address on Qubic
        id ethAddress;                       // Destination Ethereum address
        uint64 amount;                       // Amount to transfer
        uint8 orderType;                     // Type of order (e.g., mint, transfer)
        uint8 status;                        // Order status (e.g., Created, Pending, Refunded)
        bit fromQubicToEthereum;             // Direction of transfer
    };

    // Input and Output Structs
    struct createOrder_input {
        id ethAddress;
        uint64 amount;
        bit fromQubicToEthereum;
    };

    struct createOrder_output {
        uint8 status;
        array<uint8, 32> message;
    };

    struct completeOrder_input {
        uint64 orderId;
    };

    struct completeOrder_output {
        uint8 status;
        array<uint8, 32> message;
    };

    struct refundOrder_input {
        uint64 orderId;
    };

    struct refundOrder_output {
        uint8 status;
        array<uint8, 32> message;
    };

    // Order Response Structure 
    struct OrderResponse {
        uint64 orderId;                      // Order ID as uint64
        id originAccount;                    // Origin account
        id destinationAccount;               // Destination account
        uint64 amount;                       // Amount as uint64
        array<uint8, 64> memo;               // Notes or metadata
        uint32 sourceChain;                  // Source chain identifier
    };

    struct getOrder_input {
        uint64 orderId;
    };

    struct getOrder_output {
        uint8 status;
        array<uint8, 32> message;
        OrderResponse order;                 // Updated response format
    };

    // Logger structure
    struct EthBridgeLogger {
        uint32 _contractIndex;   // Index of the contract
        uint32 _errorCode;       // Error code
        uint64 _orderId;         // Order ID if applicable
        uint64 _amount;          // Amount involved in the operation
        char _terminator;        // Marks the end of the logged data
    };

    // Enum for error codes
    enum EthBridgeError {
        onlyManagersCanCompleteOrders = 1,
        invalidAmount = 2,
        insufficientTransactionFee = 3,
        orderNotFound = 4,
        invalidOrderState = 5,
        insufficientLockedTokens = 6,
        transferFailed = 7
    };


private:
    // Contract State
    QPI::HashMap<uint64, BridgeOrder, 256> orders; // Storage for orders (fixed size)
    uint64 nextOrderId;                            // Counter for order IDs
    uint64 lockedTokens;                           // Total locked tokens in the contract (balance)
    uint64 transactionFee;                         // Fee for creating an order
    id admin;                                      // Admin address
    QPI::HashMap<id, bit, 16> managers;            // Managers list
    uint64 totalReceivedTokens;                    // Total tokens received
    uint32 sourceChain;                            // Source chain identifier

    // Internal methods for admin/manager permissions        
    typedef id isAdmin_input;
    typedef bit isAdmin_output;

    PRIVATE_FUNCTION_WITH_LOCALS(isAdmin)
        output = (qpi.invocator() == state.admin);
    _

        typedef id isManager_input;
    typedef bit isManager_output;
    PRIVATE_FUNCTION_WITH_LOCALS(isManager)
        output = state.managers.get(qpi.invocator(), bit());
    _


public:
    // Create a new order and lock tokens
    struct createOrder_locals {
        BridgeOrder newOrder;
    };

    PUBLIC_PROCEDURE_WITH_LOCALS(createOrder)

        // Validate the input 
        if (input.amount == 0) {
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::invalidAmount,
                0,
                input.amount,
                '\0'
                });
            output.status = 1; // Error
            return;
        }

        if (qpi.invocationReward() < state.transactionFee) {
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::insufficientTransactionFee,
                0,
                input.amount,
                '\0'
                });
            output.status = 2; // Error
            return;
        }

        // Create the order
        locals.newOrder.orderId = state.nextOrderId++;
        locals.newOrder.qubicSender = qpi.invocator();
        locals.newOrder.ethAddress = input.ethAddress;
        locals.newOrder.amount = input.amount;
        locals.newOrder.orderType = 0; // Default order type
        locals.newOrder.status = 0; // Created
        locals.newOrder.fromQubicToEthereum = input.fromQubicToEthereum;

        // Store the order
        state.orders.set(locals.newOrder.orderId, locals.newOrder);

        LOG_INFO(EthBridgeLogger{
            CONTRACT_INDEX,
            0, // No error
            locals.newOrder.orderId,
            input.amount,
            '\0'
            });

        output.status = 0; // Success
    _

    // Retrieve an order
    struct getOrder_locals {};
    PUBLIC_FUNCTION_WITH_LOCALS(getOrder)

        BridgeOrder order;
        if (!state.orders.get(input.orderId, order)) {
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::orderNotFound,
                input.orderId,
                0, // No amount involved in this operation
                '\0'
                });
            output.status = 1; // Error
            return;
        }

        OrderResponse orderResp;
        // Populate OrderResponse with BridgeOrder data
        orderResp.orderId = order.orderId;
        orderResp.originAccount = order.qubicSender;
        orderResp.destinationAccount = order.ethAddress;
        orderResp.amount = order.amount;
        copyMemory(orderResp.memo, "Bridge transfer details"); // Placeholder for metadata
        orderResp.sourceChain = state.sourceChain;

        LOG_INFO(EthBridgeLogger{
            CONTRACT_INDEX,
            0, // No error
            order.orderId,
            order.amount,
            '\0'
            });
        output.status = 0; // Success
        output.order = orderResp;
    _

    // Admin Functions
    PUBLIC_PROCEDURE(setAdmin)

        if (qpi.invocator() != state.admin) {
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::onlyManagersCanCompleteOrders,
                0, // No order ID involved
                0, // No amount involved
                '\0'
                });
            output.status = 1; // Error
            return;
        }
        state.admin = input.address;
        LOG_INFO(EthBridgeLogger{
            CONTRACT_INDEX,
            0, // No error
            0, // No order ID involved
            0, // No amount involved
            '\0'
            });
        output.status = 0; // Success
    _

   PUBLIC_PROCEDURE(addManager)

        if (qpi.invocator() != state.admin) {
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::onlyManagersCanCompleteOrders,
                0, // No order ID involved
                0, // No amount involved
                '\0'
                });
            output.status = 1; // Error
            return;
        }

        state.managers.set(input.address, 1); // Add manager
        LOG_INFO(EthBridgeLogger{
            CONTRACT_INDEX,
            0, // No error
            0, // No order ID involved
            0, // No amount involved
            '\0'
            });
        output.status = 0; // Success
    _

   PUBLIC_PROCEDURE(removeManager)

        if (qpi.invocator() != state.admin) {
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::onlyManagersCanCompleteOrders,
                0, // No order ID involved
                0, // No amount involved
                '\0'
                });
            output.status = 1; // Error
            return;
        }

        state.managers.removeByKey(input.address); // Remove manager
        LOG_INFO(EthBridgeLogger{
            CONTRACT_INDEX,
            0, // No error
            0, // No order ID involved
            0, // No amount involved
            '\0'
            });
        output.status = 0; // Success
    _

   PUBLIC_FUNCTION_WITH_LOCALS(getTotalReceivedTokens)

        LOG_INFO(EthBridgeLogger{
            CONTRACT_INDEX,
            0, // No error
            0, // No order ID involved
            state.totalReceivedTokens, // Amount of total tokens
            '\0'
            });

        output.totalTokens = state.totalReceivedTokens;
    _

    // Complete an order and release tokens
    PUBLIC_PROCEDURE_WITH_LOCALS(completeOrder)

        bit isManager = false;
        CALL(isManager, NoData{}, isManager);

        if (!isManager) {
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::onlyManagersCanCompleteOrders,
                input.orderId,
                0,
                '\0'
                });
            output.status = 1; // Error
            return;
        }

        // Retrieve the order
        BridgeOrder order;
        if (!state.orders.get(input.orderId, order)) {
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::invalidOrderState,
                input.orderId,
                0,
                '\0'
                });
            output.status = 1; // Error
            return;
        }

        // Check the status
        if (order.status != 0) { // Ensure it's not already completed or refunded
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::invalidOrderState,
                input.orderId,
                0,
                '\0'
                });
            output.status = 2; // Error
            return;
        }

        // Handle order based on transfer direction
        if (order.fromQubicToEthereum) {
            // Ensure sufficient tokens were transferred to the contract
            if (state.totalReceivedTokens < order.amount) {
                LOG_INFO(EthBridgeLogger{
                    CONTRACT_INDEX,
                    EthBridgeError::insufficientLockedTokens,
                    input.orderId,
                    order.amount,
                    '\0'
                    });
                output.status = 4; // Error
                return;
            }

            state.lockedTokens += order.amount;
        }
        else {
            // Ensure sufficient tokens are locked for the order
            if (state.lockedTokens < order.amount) {
                LOG_INFO(EthBridgeLogger{
                    CONTRACT_INDEX,
                    EthBridgeError::insufficientLockedTokens,
                    input.orderId,
                    order.amount,
                    '\0'
                    });
                output.status = 5; // Error
                return;
            }

            // Transfer tokens back to the user
            if (qpi.transfer(order.qubicSender, order.amount) < 0) {
                LOG_INFO(EthBridgeLogger{
                    CONTRACT_INDEX,
                    EthBridgeError::transferFailed,
                    input.orderId,
                    order.amount,
                    '\0'
                    });
                output.status = 6; // Error
                return;
            }

            state.lockedTokens -= order.amount;
        }

        // Mark the order as completed
        order.status = 1; // Completed
        state.orders.set(order.orderId, order);

        output.status = 0; // Success
        LOG_INFO(EthBridgeLogger{
            CONTRACT_INDEX,
            0, // No error
            input.orderId,
            order.amount,
            '\0'
            });
    _

    // Refund an order and unlock tokens
    PUBLIC_PROCEDURE_WITH_LOCALS(refundOrder)

        bit isManager = false;
        CALL(isManager, NoData{}, isManager);
        if (!isManager) {
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::orderNotFound,
                input.orderId,
                0, // No amount involved
                '\0'
            });
            output.status = 1; // Error
            return;
        }

        // Retrieve the order
        BridgeOrder order;
        if (!state.orders.get(input.orderId, order)) {
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::invalidOrderState,
                input.orderId,
                0, // No amount involved
                '\0'
            });
            output.status = 1; // Error
            return;
        }

        // Check the status
        if (order.status != 0) { // Ensure it's not already completed or refunded
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::transferFailed,
                input.orderId,
                order.amount,
                '\0'
            });
            output.status = 2; // Error
            return;
        }

        // Update the status and refund tokens
        qpi.transfer(order.qubicSender, order.amount);
        state.lockedTokens -= order.amount;
        order.status = 2; // Refunded
        state.orders.set(order.orderId, order);

        LOG_INFO(EthBridgeLogger{
            CONTRACT_INDEX,
            0, // No error
            input.orderId,
            order.amount,
            '\0'
        });
        output.status = 0; // Success
    _

    // Transfer tokens to the contract
    PUBLIC_PROCEDURE_WITH_LOCALS(transferToContract)

        if (input.amount == 0) {
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::invalidAmount,
                0, // No order ID
                input.amount,
                '\0'
            });
            output.status = 1; // Error
            return;
        }

        if (state.userBalances.get(qpi.invocator(), 0) < input.amount) {
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                EthBridgeError::insufficientLockedTokens,
                0, // No order ID
                input.amount,
                '\0'
                });
            output.status = 3; // Error
            return;
        }
        else {

            if (qpi.transfer(SELF, input.amount) < 0) {
                output.status = 2; // Error
                LOG_INFO(EthBridgeLogger{
                    CONTRACT_INDEX,
                    EthBridgeError::transferFailed,
                    0, // No order ID
                    input.amount,
                    '\0'
                });
                return;
            }

            // Update the total received tokens
            state.totalReceivedTokens += input.amount;
            LOG_INFO(EthBridgeLogger{
                CONTRACT_INDEX,
                0, // No error
                0, // No order ID
                input.amount,
                '\0'
            });
            output.status = 0; // Success
        }
    _

    // Register Functions and Procedures
    REGISTER_USER_FUNCTIONS_AND_PROCEDURES
        REGISTER_USER_PROCEDURE(createOrder, 1);
        REGISTER_USER_FUNCTION(getOrder, 2);

        REGISTER_USER_PROCEDURE(setAdmin, 3);
        REGISTER_USER_PROCEDURE(addManager, 4);
        REGISTER_USER_PROCEDURE(removeManager, 5);

        REGISTER_USER_PROCEDURE(completeOrder, 6);
        REGISTER_USER_PROCEDURE(refundOrder, 7);
        REGISTER_USER_PROCEDURE(transferToContract, 8);
    _

    // Initialize the contract
    INITIALIZE
        state.nextOrderId = 0;
        state.lockedTokens = 0;
        state.totalReceivedTokens = 0;
        state.transactionFee = 1000;
        // Let's try to set admin as the contract creator, not the contract owner
        state.admin = qpi.invocator(); //If this fails, set a predetermined address
        state.managers.reset(); // Initialize managers list
        state.sourceChain = 0; //Arbitrary numb. No-EVM chain
    _
};
